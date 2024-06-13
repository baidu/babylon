// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

////////////////////////////////////////////////////////////////////////////////
// 以下主体拷贝自
// https://github.com/apache/brpc/blob/1.9.0/src/brpc/policy/baidu_rpc_protocol.cpp
// 修改其中正反序列化相关部分
// clang-format off

#include "reusable_rpc_protocol.h"

#include <google/protobuf/descriptor.h>         // MethodDescriptor
#include <google/protobuf/message.h>            // Message
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/io/coded_stream.h>
#include "butil/logging.h"                       // LOG()
#include "butil/time.h"
#include "butil/iobuf.h"                         // butil::IOBuf
#include "butil/raw_pack.h"                      // RawPacker RawUnpacker
#include "brpc/controller.h"                    // Controller
#include "brpc/socket.h"                        // Socket
#include "brpc/server.h"                        // Server
#include "brpc/span.h"
#include "brpc/compress.h"                      // ParseFromCompressedData
#include "brpc/stream_impl.h"
#include "brpc/rpc_dump.h"                      // SampledRequest
#include "brpc/policy/baidu_rpc_meta.pb.h"      // RpcRequestMeta
#include "brpc/policy/baidu_rpc_protocol.h"
#include "brpc/policy/most_common_message.h"
#include "brpc/policy/streaming_rpc_protocol.h"
#include "brpc/details/usercode_backup_pool.h"
#include "brpc/details/controller_private_accessor.h"
#include "brpc/details/server_private_accessor.h"

extern "C" {
void bthread_assign_data(void* data);
}


namespace brpc {
namespace policy {

// Notes:
// 1. 12-byte header [PRPC][body_size][meta_size]
// 2. body_size and meta_size are in network byte order
// 3. Use service->full_name() + method_name to specify the method to call
// 4. `attachment_size' is set iff request/response has attachment
// 5. Not supported: chunk_info

// Pack header into `buf'
inline void PackRpcHeader(char* rpc_header, uint32_t meta_size, int payload_size) {
    uint32_t* dummy = (uint32_t*)rpc_header;  // suppress strict-alias warning
    *dummy = *(uint32_t*)"PRPC";
    butil::RawPacker(rpc_header + 4)
        .pack32(meta_size + payload_size)
        .pack32(meta_size);
}

static void SerializeRpcHeaderAndMeta(
    butil::IOBuf* out, const RpcMeta& meta, int payload_size) {
    const uint32_t meta_size = GetProtobufByteSize(meta);
    if (meta_size <= 244) { // most common cases
        char header_and_meta[12 + meta_size];
        PackRpcHeader(header_and_meta, meta_size, payload_size);
        ::google::protobuf::io::ArrayOutputStream arr_out(header_and_meta + 12, meta_size);
        ::google::protobuf::io::CodedOutputStream coded_out(&arr_out);
        meta.SerializeWithCachedSizes(&coded_out); // not calling ByteSize again
        CHECK(!coded_out.HadError());
        CHECK_EQ(0, out->append(header_and_meta, sizeof(header_and_meta)));
    } else {
        char header[12];
        PackRpcHeader(header, meta_size, payload_size);
        CHECK_EQ(0, out->append(header, sizeof(header)));
        butil::IOBufAsZeroCopyOutputStream buf_stream(out);
        ::google::protobuf::io::CodedOutputStream coded_out(&buf_stream);
        meta.SerializeWithCachedSizes(&coded_out);
        CHECK(!coded_out.HadError());
    }
}


// Used by UT, can't be static.
static void SendRpcResponseReuse(int64_t correlation_id,
                                 Controller* cntl, 
                                 const google::protobuf::Message* req,
                                 const google::protobuf::Message* res,
                                 const Server* server,
                                 MethodStatus* method_status,
                                 int64_t received_us) {
    ControllerPrivateAccessor accessor(cntl);
    Span* span = accessor.span();
    if (span) {
        span->set_start_send_us(butil::cpuwide_time_us());
    }
    Socket* sock = accessor.get_sending_socket();

    //std::unique_ptr<const google::protobuf::Message> recycle_req(req);
    //std::unique_ptr<const google::protobuf::Message> recycle_res(res);

    std::unique_ptr<Controller, LogErrorTextAndDelete> recycle_cntl(cntl);
    ConcurrencyRemover concurrency_remover(method_status, cntl, received_us);

    ClosureGuard guard(brpc::NewCallback(cntl, &Controller::CallAfterRpcResp, req, res));
    
    StreamId response_stream_id = accessor.response_stream();

    if (cntl->IsCloseConnection()) {
        StreamClose(response_stream_id);
        sock->SetFailed();
        return;
    }
    bool append_body = false;
    butil::IOBuf res_body;
    // `res' can be NULL here, in which case we don't serialize it
    // If user calls `SetFailed' on Controller, we don't serialize
    // response either
    CompressType type = cntl->response_compress_type();
    if (res != NULL && !cntl->Failed()) {
        if (!res->IsInitialized()) {
            cntl->SetFailed(
                ERESPONSE, "Missing required fields in response: %s", 
                res->InitializationErrorString().c_str());
        } else if (!SerializeAsCompressedData(*res, &res_body, type)) {
            cntl->SetFailed(ERESPONSE, "Fail to serialize response, "
                            "CompressType=%s", CompressTypeToCStr(type));
        } else {
            append_body = true;
        }
    }

    // Don't use res->ByteSize() since it may be compressed
    size_t res_size = 0;
    size_t attached_size = 0;
    if (append_body) {
        res_size = res_body.length();
        attached_size = cntl->response_attachment().length();
    }

    int error_code = cntl->ErrorCode();
    if (error_code == -1) {
        // replace general error (-1) with INTERNAL_SERVER_ERROR to make a
        // distinction between server error and client error
        error_code = EINTERNAL;
    }
    RpcMeta meta;
    RpcResponseMeta* response_meta = meta.mutable_response();
    response_meta->set_error_code(error_code);
    if (!cntl->ErrorText().empty()) {
        // Only set error_text when it's not empty since protobuf Message
        // always new the string no matter if it's empty or not.
        response_meta->set_error_text(cntl->ErrorText());
    }
    meta.set_correlation_id(correlation_id);
    meta.set_compress_type(cntl->response_compress_type());
    if (attached_size > 0) {
        meta.set_attachment_size(attached_size);
    }
    SocketUniquePtr stream_ptr;
    if (response_stream_id != INVALID_STREAM_ID) {
        if (Socket::Address(response_stream_id, &stream_ptr) == 0) {
            Stream* s = (Stream*)stream_ptr->conn();
            s->FillSettings(meta.mutable_stream_settings());
            s->SetHostSocket(sock);
        } else {
            LOG(WARNING) << "Stream=" << response_stream_id 
                         << " was closed before sending response";
        }
    }

    if (cntl->has_response_user_fields() &&
        !cntl->response_user_fields()->empty()) {
        ::google::protobuf::Map<std::string, std::string>& user_fields
            = *meta.mutable_user_fields();
        user_fields.insert(cntl->response_user_fields()->begin(),
                           cntl->response_user_fields()->end());

    }

    butil::IOBuf res_buf;
    SerializeRpcHeaderAndMeta(&res_buf, meta, res_size + attached_size);
    if (append_body) {
        res_buf.append(res_body.movable());
        if (attached_size) {
            res_buf.append(cntl->response_attachment().movable());
        }
    }

    if (span) {
        span->set_response_size(res_buf.size());
    }
    // Send rpc response over stream even if server side failed to create
    // stream for some reason.
    if(cntl->has_remote_stream()){
        // Send the response over stream to notify that this stream connection
        // is successfully built.
        // Response_stream can be INVALID_STREAM_ID when error occurs.
        if (SendStreamData(sock, &res_buf,
                           accessor.remote_stream_settings()->stream_id(),
                           accessor.response_stream()) != 0) {
            const int errcode = errno;
            std::string error_text = butil::string_printf(64, "Fail to write into %s",
                                                          sock->description().c_str());
            PLOG_IF(WARNING, errcode != EPIPE) << error_text;
            cntl->SetFailed(errcode,  "%s", error_text.c_str());
            if(stream_ptr) {
                ((Stream*)stream_ptr->conn())->Close(errcode, "%s",
                                                     error_text.c_str());
            }
            return;
        }

        if(stream_ptr) {
            // Now it's ok the mark this server-side stream as connected as all the
            // written user data would follower the RPC response.
            ((Stream*)stream_ptr->conn())->SetConnected();
        }
    } else{
        // Have the risk of unlimited pending responses, in which case, tell
        // users to set max_concurrency.
        Socket::WriteOptions wopt;
        wopt.ignore_eovercrowded = true;
        if (sock->Write(&res_buf, &wopt) != 0) {
            const int errcode = errno;
            PLOG_IF(WARNING, errcode != EPIPE) << "Fail to write into " << *sock;
            cntl->SetFailed(errcode, "Fail to write into %s",
                            sock->description().c_str());
            return;
        }
    }

    if (span) {
        // TODO: this is not sent
        span->set_sent_us(butil::cpuwide_time_us());
    }
}

// Used by other protocols as well.
void EndRunningCallMethodInPool(
    ::google::protobuf::Service* service,
    const ::google::protobuf::MethodDescriptor* method,
    ::google::protobuf::RpcController* controller,
    const ::google::protobuf::Message* request,
    ::google::protobuf::Message* response,
    ::google::protobuf::Closure* done);

static void ProcessRpcRequestReused(InputMessageBase* msg_base) {
    const int64_t start_parse_us = butil::cpuwide_time_us();
    DestroyingPtr<MostCommonMessage> msg(static_cast<MostCommonMessage*>(msg_base));
    SocketUniquePtr socket_guard(msg->ReleaseSocket());
    Socket* socket = socket_guard.get();
    const Server* server = static_cast<const Server*>(msg_base->arg());
    ScopedNonServiceError non_service_error(server);

    RpcMeta meta;
    if (!ParsePbFromIOBuf(&meta, msg->meta)) {
        LOG(WARNING) << "Fail to parse RpcMeta from " << *socket;
        socket->SetFailed(EREQUEST, "Fail to parse RpcMeta from %s",
                          socket->description().c_str());
        return;
    }
    const RpcRequestMeta &request_meta = meta.request();

    SampledRequest* sample = AskToBeSampled();
    if (sample) {
        sample->meta.set_service_name(request_meta.service_name());
        sample->meta.set_method_name(request_meta.method_name());
        sample->meta.set_compress_type((CompressType)meta.compress_type());
        sample->meta.set_protocol_type(PROTOCOL_BAIDU_STD);
        sample->meta.set_attachment_size(meta.attachment_size());
        sample->meta.set_authentication_data(meta.authentication_data());
        sample->request = msg->payload;
        sample->submit(start_parse_us);
    }

    std::unique_ptr<Controller> cntl(new (std::nothrow) Controller);
    if (NULL == cntl.get()) {
        LOG(WARNING) << "Fail to new Controller";
        return;
    }

    struct NoDelete {
        void operator()(::google::protobuf::Message*) {}
    };
    std::unique_ptr<google::protobuf::Message, NoDelete> req;
    std::unique_ptr<google::protobuf::Message, NoDelete> res;
    babylon::ReusableRPCProtocol::Closure* done = nullptr;

    ServerPrivateAccessor server_accessor(server);
    ControllerPrivateAccessor accessor(cntl.get());
    const bool security_mode = server->options().security_mode() &&
                               socket->user() == server_accessor.acceptor();
    if (request_meta.has_log_id()) {
        cntl->set_log_id(request_meta.log_id());
    }
    if (request_meta.has_request_id()) {
        cntl->set_request_id(request_meta.request_id());
    }
    if (request_meta.has_timeout_ms()) {
        cntl->set_timeout_ms(request_meta.timeout_ms());
    }
    cntl->set_request_compress_type((CompressType)meta.compress_type());
    accessor.set_server(server)
        .set_security_mode(security_mode)
        .set_peer_id(socket->id())
        .set_remote_side(socket->remote_side())
        .set_local_side(socket->local_side())
        .set_auth_context(socket->auth_context())
        .set_request_protocol(PROTOCOL_BAIDU_STD)
        .set_begin_time_us(msg->received_us())
        .move_in_server_receiving_sock(socket_guard);

    if (meta.has_stream_settings()) {
        accessor.set_remote_stream_settings(meta.release_stream_settings());
    }

    if (!meta.user_fields().empty()) {
        for (const auto& it : meta.user_fields()) {
            (*cntl->request_user_fields())[it.first] = it.second;
        }
    }

    // Tag the bthread with this server's key for thread_local_data().
    if (server->thread_local_options().thread_local_data_factory) {
        bthread_assign_data((void*)&server->thread_local_options());
    }

    Span* span = NULL;
    if (IsTraceable(request_meta.has_trace_id())) {
        span = Span::CreateServerSpan(
            request_meta.trace_id(), request_meta.span_id(),
            request_meta.parent_span_id(), msg->base_real_us());
        accessor.set_span(span);
        span->set_log_id(request_meta.log_id());
        span->set_remote_side(cntl->remote_side());
        span->set_protocol(PROTOCOL_BAIDU_STD);
        span->set_received_us(msg->received_us());
        span->set_start_parse_us(start_parse_us);
        span->set_request_size(msg->payload.size() + msg->meta.size() + 12);
    }

    MethodStatus* method_status = NULL;
    do {
        if (!server->IsRunning()) {
            cntl->SetFailed(ELOGOFF, "Server is stopping");
            break;
        }

        if (socket->is_overcrowded()) {
            cntl->SetFailed(EOVERCROWDED, "Connection to %s is overcrowded",
                            butil::endpoint2str(socket->remote_side()).c_str());
            break;
        }
        
        if (!server_accessor.AddConcurrency(cntl.get())) {
            cntl->SetFailed(
                ELIMIT, "Reached server's max_concurrency=%d",
                server->options().max_concurrency);
            break;
        }

        if (FLAGS_usercode_in_pthread && TooManyUserCode()) {
            cntl->SetFailed(ELIMIT, "Too many user code to run when"
                            " -usercode_in_pthread is on");
            break;
        }

        // NOTE(gejun): jprotobuf sends service names without packages. So the
        // name should be changed to full when it's not.
        butil::StringPiece svc_name(request_meta.service_name());
        if (svc_name.find('.') == butil::StringPiece::npos) {
            const Server::ServiceProperty* sp =
                server_accessor.FindServicePropertyByName(svc_name);
            if (NULL == sp) {
                cntl->SetFailed(ENOSERVICE, "Fail to find service=%s",
                                request_meta.service_name().c_str());
                break;
            }
            svc_name = sp->service->GetDescriptor()->full_name();
        }
        const Server::MethodProperty* mp =
            server_accessor.FindMethodPropertyByFullName(
                svc_name, request_meta.method_name());
        if (NULL == mp) {
            cntl->SetFailed(ENOMETHOD, "Fail to find method=%s/%s",
                            request_meta.service_name().c_str(),
                            request_meta.method_name().c_str());
            break;
        } else if (mp->service->GetDescriptor()
                   == BadMethodService::descriptor()) {
            BadMethodRequest breq;
            BadMethodResponse bres;
            breq.set_service_name(request_meta.service_name());
            mp->service->CallMethod(mp->method, cntl.get(), &breq, &bres, NULL);
            break;
        }
        // Switch to service-specific error.
        non_service_error.release();
        method_status = mp->status;
        if (method_status) {
            int rejected_cc = 0;
            if (!method_status->OnRequested(&rejected_cc, cntl.get())) {
                cntl->SetFailed(ELIMIT, "Rejected by %s's ConcurrencyLimiter, concurrency=%d",
                                mp->method->full_name().c_str(), rejected_cc);
                break;
            }
        }
        google::protobuf::Service* svc = mp->service;
        const google::protobuf::MethodDescriptor* method = mp->method;
        accessor.set_method(method);


        if (!server->AcceptRequest(cntl.get())) {
            break;
        }

        if (span) {
            span->ResetServerSpanName(method->full_name());
        }
        const int req_size = static_cast<int>(msg->payload.size());
        butil::IOBuf req_buf;
        butil::IOBuf* req_buf_ptr = &msg->payload;
        if (meta.has_attachment_size()) {
            if (req_size < meta.attachment_size()) {
                cntl->SetFailed(EREQUEST,
                    "attachment_size=%d is larger than request_size=%d",
                     meta.attachment_size(), req_size);
                break;
            }
            int body_without_attachment_size = req_size - meta.attachment_size();
            msg->payload.cutn(&req_buf, body_without_attachment_size);
            req_buf_ptr = &req_buf;
            cntl->request_attachment().swap(msg->payload);
        }

        CompressType req_cmp_type = (CompressType)meta.compress_type();
        done = babylon::ReusableRPCProtocol::create(mp);
        done->set_correlation_id(meta.correlation_id());
        done->set_received_us(msg->received_us());
        done->set_server(server);
        done->set_controller(cntl.get());
        req.reset(done->request());
        if (!ParseFromCompressedData(*req_buf_ptr, req.get(), req_cmp_type)) {
            cntl->SetFailed(EREQUEST, "Fail to parse request message, "
                            "CompressType=%s, request_size=%d", 
                            CompressTypeToCStr(req_cmp_type), req_size);
            break;
        }
        
        res.reset(done->response());
        // `socket' will be held until response has been sent
        //google::protobuf::Closure* done = ::brpc::NewCallback<
        //    int64_t, Controller*, const google::protobuf::Message*,
        //    const google::protobuf::Message*, const Server*,
        //    MethodStatus*, int64_t>(
        //        &SendRpcResponse, meta.correlation_id(), cntl.get(), 
        //        req.get(), res.get(), server,
        //        method_status, msg->received_us());

        // optional, just release resource ASAP
        msg.reset();
        req_buf.clear();

        if (span) {
            span->set_start_callback_us(butil::cpuwide_time_us());
            span->AsParent();
        }
        if (!FLAGS_usercode_in_pthread) {
            return svc->CallMethod(method, cntl.release(), 
                                   req.release(), res.release(), done);
        }
        if (BeginRunningUserCode()) {
            svc->CallMethod(method, cntl.release(), 
                            req.release(), res.release(), done);
            return EndRunningUserCodeInPlace();
        } else {
            return EndRunningCallMethodInPool(
                svc, method, cntl.release(),
                req.release(), res.release(), done);
        }
    } while (false);
    
    // `cntl', `req' and `res' will be deleted inside `SendRpcResponse'
    // `socket' will be held until response has been sent
    if (done != nullptr) {
        done->Run();
    } else {
        SendRpcResponseReuse(meta.correlation_id(), cntl.release(), 
                             req.release(), res.release(), server,
                             method_status, msg->received_us());
    }
}

}  // namespace policy
} // namespace brpc

// clang-format on
// 以上内容主体拷贝自
// https://github.com/apache/brpc/blob/1.9.0/src/brpc/policy/baidu_rpc_protocol.cpp
// 修改其中正反序列化相关部分
////////////////////////////////////////////////////////////////////////////////

DECLARE_bool(babylon_rpc_full_reuse);

BABYLON_NAMESPACE_BEGIN

ReusableRPCProtocol::ReusableRPCProtocol(const char* protocol_name) noexcept {
  parse = ::brpc::policy::ParseRpcMessage;
  serialize_request = nullptr;
  pack_request = nullptr;
  process_request = ::brpc::policy::ProcessRpcRequestReused;
  verify = ::brpc::policy::VerifyRpcRequest;
  parse_server_address = nullptr;
  get_method_name = nullptr;
  supported_connection_type = ::brpc::CONNECTION_TYPE_ALL;
  name = protocol_name;
}

void ReusableRPCProtocol::Closure::Run() noexcept {
  ::brpc::policy::SendRpcResponseReuse(_correlation_id, _controller, _request,
                                       _response, _server, _method_status,
                                       _received_us);
  if (FLAGS_babylon_rpc_full_reuse) {
    _manager.clear();
  } else {
    resource().release();
  }
  _pool->push(::std::unique_ptr<Closure> {this});
}

BABYLON_NAMESPACE_END
