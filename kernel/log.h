/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Clifford Wolf <clifford@clifford.at>
 *  
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifndef LOG_H
#define LOG_H

#include "kernel/rtlil.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <vector>

extern std::vector<FILE*> log_files;
extern FILE *log_errfile;
extern bool log_time;
extern bool log_cmd_error_throw;
extern int log_verbose_level;

std::string stringf(const char *fmt, ...);

void logv(const char *format, va_list ap);
void logv_header(const char *format, va_list ap);
void logv_error(const char *format, va_list ap) __attribute__ ((noreturn));

void log(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));
void log_header(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
void log_error(const char *format, ...) __attribute__ ((format (printf, 1, 2))) __attribute__ ((noreturn));
void log_cmd_error(const char *format, ...) __attribute__ ((format (printf, 1, 2))) __attribute__ ((noreturn));

void log_push();
void log_pop();

void log_reset_stack();
void log_flush();

const char *log_signal(const RTLIL::SigSpec &sig, bool autoint = true);

#define log_abort() log_error("Abort in %s:%d.\n", __FILE__, __LINE__)
#define log_assert(_assert_expr_) do { if (_assert_expr_) break; log_error("Assert `%s' failed in %s:%d.\n", #_assert_expr_, __FILE__, __LINE__); } while (0)

// simple timer for performance measurements
// toggle the '#if 1' to get a baseline for the perormance penalty added by the measurement
struct PerformanceTimer
{
#if 1
	int64_t total_ns;

	PerformanceTimer() {
		total_ns = 0;
	}

	static int64_t query() {
#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
		struct timespec ts;
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
		return int64_t(ts.tv_sec)*1000000000 + ts.tv_nsec;
#elif defined(RUSAGE_SELF)
		struct rusage rusage;
		int64_t t;
		if (getrusage(RUSAGE_SELF, &rusage) == -1) {
			log_cmd_error("getrusage failed!\n");
			log_abort();
		}
		t = 1000000000ULL * (int64_t) rusage.ru_utime.tv_sec + (int64_t) rusage.ru_utime.tv_usec * 1000ULL;
		t += 1000000000ULL * (int64_t) rusage.ru_stime.tv_sec + (int64_t) rusage.ru_stime.tv_usec * 1000ULL;
		return t;
#else
	#error Dont know how to measure per-process CPU time. Need alternative method (times()/clocks()/gettimeofday()?).
#endif
	}

	void reset() {
		total_ns = 0;
	}

	void add() {
		total_ns += query();
	}

	void sub() {
		total_ns -= query();
	}

	float sec() const {
		return total_ns * 1e-9;
	}
#else
	void reset() { }
	void add() { }
	void sub() { }
	float sec() const { return 0; }
#endif
};

// simple API for quickly dumping values when debugging

static inline void log_dump_val_worker(short v) { log("%d", v); }
static inline void log_dump_val_worker(unsigned short v) { log("%u", v); }
static inline void log_dump_val_worker(int v) { log("%d", v); }
static inline void log_dump_val_worker(unsigned int v) { log("%u", v); }
static inline void log_dump_val_worker(long int v) { log("%ld", v); }
static inline void log_dump_val_worker(unsigned long int v) { log("%lu", v); }
static inline void log_dump_val_worker(long long int v) { log("%lld", v); }
static inline void log_dump_val_worker(unsigned long long int v) { log("%lld", v); }
static inline void log_dump_val_worker(char c) { log(c >= 32 && c < 127 ? "'%c'" : "'\\x%02x'", c); }
static inline void log_dump_val_worker(unsigned char c) { log(c >= 32 && c < 127 ? "'%c'" : "'\\x%02x'", c); }
static inline void log_dump_val_worker(bool v) { log("%s", v ? "true" : "false"); }
static inline void log_dump_val_worker(double v) { log("%f", v); }
static inline void log_dump_val_worker(const char *v) { log("%s", v); }
static inline void log_dump_val_worker(std::string v) { log("%s", v.c_str()); }
static inline void log_dump_val_worker(RTLIL::SigSpec v) { log("%s", log_signal(v)); }
static inline void log_dump_args_worker(const char *p) { log_assert(*p == 0); }

template<typename T>
static inline void log_dump_val_worker(T *ptr) { log("%p", ptr); }

template <typename T, typename ... Args>
void log_dump_args_worker(const char *p, T first, Args ... args)
{
	int next_p_state = 0;
	const char *next_p = p;
	while (*next_p && (next_p_state != 0 || *next_p != ',')) {
		if (*next_p == '"')
			do {
				next_p++;
				while (*next_p == '\\' && *(next_p + 1))
					next_p += 2;
			} while (*next_p && *next_p != '"');
		if (*next_p == '\'') {
			next_p++;
			if (*next_p == '\\')
				next_p++;
			if (*next_p)
				next_p++;
		}
		if (*next_p == '(' || *next_p == '[' || *next_p == '{')
			next_p_state++;
		if ((*next_p == ')' || *next_p == ']' || *next_p == '}') && next_p_state > 0)
			next_p_state--;
		next_p++;
	}
	log("\n\t%.*s => ", int(next_p - p), p);
	if (*next_p == ',')
		next_p++;
	while (*next_p == ' ' || *next_p == '\t' || *next_p == '\r' || *next_p == '\n')
		next_p++;
	log_dump_val_worker(first);
	log_dump_args_worker(next_p, args ...);
}

#define log_dump(...) do { \
	log("DEBUG DUMP IN %s AT %s:%d:", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
	log_dump_args_worker(#__VA_ARGS__, __VA_ARGS__); \
	log("\n"); \
} while (0)

#endif
