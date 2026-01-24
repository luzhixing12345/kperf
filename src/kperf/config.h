
#pragma once

#define KPERF_RESULTS_PATH "kperf-result"

#ifdef RELEASE
#define KPERF_ETC_TPL_PATH "/etc/kperf"
#else
#define KPERF_ETC_TPL_PATH "assets"
#endif