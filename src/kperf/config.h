
#pragma once

#define KPERF_RESULT_PATH "kperf.html"

#ifdef RELEASE
#define KPERF_ETC_TPL_PATH "/etc/kperf"
#else
#define KPERF_ETC_TPL_PATH "assets"
#endif