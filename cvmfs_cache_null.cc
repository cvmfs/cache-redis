/**
 * This file is part of the CernVM File System.
 *
 * A demo external cache plugin.  All data is stored in std::string.  Feature-
 * complete but quite inefficient.
 */

#define __STDC_FORMAT_MACROS

#include <alloca.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "libcvmfs_cache.h"

using namespace std;  // NOLINT

int main(int argc, char **argv) {
  if (argc < 2) {
    Usage(argv[0]);
    return 1;
  }

  cvmcache_init_global();

  cvmcache_option_map *options = cvmcache_options_init();
  if (cvmcache_options_parse(options, argv[1]) != 0) {
    printf("cannot parse options file %s\n", argv[1]);
    return 1;
  }
  char *locator = cvmcache_options_get(options, "CVMFS_CACHE_PLUGIN_LOCATOR");
  if (locator == NULL) {
    printf("CVMFS_CACHE_PLUGIN_LOCATOR missing\n");
    cvmcache_options_fini(options);
    return 1;
  }
  char *test_mode = cvmcache_options_get(options, "CVMFS_CACHE_PLUGIN_TEST");

  if (!test_mode)
    cvmcache_spawn_watchdog(NULL);

  struct cvmcache_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.cvmcache_chrefcnt = null_chrefcnt;
  callbacks.cvmcache_obj_info = null_obj_info;
  callbacks.cvmcache_pread = null_pread;
  callbacks.cvmcache_start_txn = null_start_txn;
  callbacks.cvmcache_write_txn = null_write_txn;
  callbacks.cvmcache_commit_txn = null_commit_txn;
  callbacks.cvmcache_abort_txn = null_abort_txn;
  callbacks.cvmcache_info = null_info;
  callbacks.cvmcache_shrink = null_shrink;
  callbacks.cvmcache_listing_begin = null_listing_begin;
  callbacks.cvmcache_listing_next = null_listing_next;
  callbacks.cvmcache_listing_end = null_listing_end;
  callbacks.capabilities = CVMCACHE_CAP_ALL_V1;

  ctx = cvmcache_init(&callbacks);
  int retval = cvmcache_listen(ctx, locator);
  if (!retval) {
    fprintf(stderr, "failed to listen on %s\n", locator);
    return 1;
  }

  if (test_mode) {
    // Daemonize, print out PID
    pid_t pid;
    int statloc;
    if ((pid = fork()) == 0) {
      if ((pid = fork()) == 0) {
        int null_read = open("/dev/null", O_RDONLY);
        int null_write = open("/dev/null", O_WRONLY);
        assert((null_read >= 0) && (null_write >= 0));
        int retval = dup2(null_read, 0);
        assert(retval == 0);
        retval = dup2(null_write, 1);
        assert(retval == 1);
        retval = dup2(null_write, 2);
        assert(retval == 2);
        close(null_read);
        close(null_write);
      } else {
        assert(pid > 0);
        printf("%d\n", pid);
        fflush(stdout);
        fsync(1);
        _exit(0);
      }
    } else {
      assert(pid > 0);
      waitpid(pid, &statloc, 0);
      _exit(0);
    }
  }

  printf("Listening for cvmfs clients on %s\n", locator);
  printf("NOTE: this process needs to run as user cvmfs\n\n");

  // Starts the I/O processing thread
  cvmcache_process_requests(ctx, 0);

  if (test_mode)
    while (true) sleep(1);

  if (!cvmcache_is_supervised()) {
    printf("Press <R ENTER> to ask clients to release nested catalogs\n");
    printf("Press <Ctrl+D> to quit\n");
    while (true) {
      char buf;
      int retval = read(fileno(stdin), &buf, 1);
      if (retval != 1)
        break;
      if (buf == 'R') {
        printf("  ... asking clients to release nested catalogs\n");
        cvmcache_ask_detach(ctx);
      }
    }
    cvmcache_terminate(ctx);
  }

  cvmcache_wait_for(ctx);
  printf("  ... good bye\n");

  cvmcache_options_free(locator);
  cvmcache_options_fini(options);
  cvmcache_terminate_watchdog();
  cvmcache_cleanup_global();
  return 0;
}
