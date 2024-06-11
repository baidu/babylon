#!/bin/bash
set -ex

grep -Pn 'BABYLON_LOG\(.*\)' src/ -R | awk -F: '{print $1":"$2}' > ignore_branches

awk -F: '
ARGIND == 1 {skip_lines[$1] = skip_lines[$1]""$2","}
ARGIND == 2 {
  skip = 0;
}
ARGIND == 2 && $1 == "SF" {
  n = split(skip_lines[$2], arr, ",");
  delete skip_line_set;
  for (i = 1; i < n; ++i) {
    skip_line_set[arr[i]];
  }
  skip_brf = 0;
  skip_brh = 0;
}
ARGIND == 2 && $1 == "BRDA" {
  split($2, arr, ",");
  if (arr[1] in skip_line_set) {
    skip = 1;
    skip_brf++;
    if (arr[4] > 0) {
      skip_brh++;
    }
  }
}
ARGIND == 2 && $1 == "BRF" && skip_brf {
  skip = 1;
  print $1":"$2-skip_brf
}
ARGIND == 2 && $1 == "BRH" && skip_brh {
  skip = 1;
  print $1":"$2-skip_brh
}
ARGIND == 2 && !skip {
  print
}
' ignore_branches bazel-out/_coverage/_coverage_report.dat > bazel-out/_coverage/_coverage_report.lcov
