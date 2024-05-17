def module_name():
  return native.module_name() if ('module_name' in dir(native)) else None
