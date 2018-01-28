#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "redis_plugin.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    RedisPlugin::Usage(argv[0]);
    return 1;
  }

  cvmcache_init_global();

  cvmcache_option_map *options = cvmcache_options_init();
  if (cvmcache_options_parse(options, argv[1]) != 0) {
    printf("cannot parse options file %s\n", argv[1]);
    return 1;
  }
  char *locator = cvmcache_options_get(options, "CVMFS_CACHE_redis_LOCATOR");
  if (locator == NULL) {
    printf("CVMFS_CACHE_redis_LOCATOR missing\n");
    cvmcache_options_fini(options);
    return 1;
  }

  std::string redis_server_address(
    cvmcache_options_get(options, "CVMFS_CACHE_redis_REDIS_SERVER_ADDRESS"));
  int redis_server_port = std::atoi(
    cvmcache_options_get(options, "CVMFS_CACHE_redis_REDIS_SERVER_PORT"));

  auto ret = RedisPlugin::Connect(redis_server_address, redis_server_port);
  assert(ret);

  struct cvmcache_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.cvmcache_chrefcnt = RedisPlugin::redis_chrefcnt;
  callbacks.cvmcache_obj_info = RedisPlugin::redis_obj_info;
  callbacks.cvmcache_pread = RedisPlugin::redis_pread;
  callbacks.cvmcache_start_txn = RedisPlugin::redis_start_txn;
  callbacks.cvmcache_write_txn = RedisPlugin::redis_write_txn;
  callbacks.cvmcache_commit_txn = RedisPlugin::redis_commit_txn;
  callbacks.cvmcache_abort_txn = RedisPlugin::redis_abort_txn;
  //callbacks.cvmcache_info = RedisPlugin::redis_info;
  //callbacks.cvmcache_shrink = RedisPlugin::redis_shrink;
  //callbacks.cvmcache_listing_begin = RedisPlugin::redis_listing_begin;
  //callbacks.cvmcache_listing_next = RedisPlugin::redis_listing_next;
  //callbacks.cvmcache_listing_end = RedisPlugin::redis_listing_end;
  callbacks.capabilities = RedisPlugin::Capabilities();

  struct cvmcache_context *ctx = cvmcache_init(&callbacks);
  int retval = cvmcache_listen(ctx, locator);
  if (!retval) {
    fprintf(stderr, "failed to listen on %s\n", locator);
    return 1;
  }

  printf("Listening for cvmfs clients on %s\n", locator);
  printf("NOTE: this process needs to run as user cvmfs\n\n");

  // Starts the I/O processing thread
  cvmcache_process_requests(ctx, 0);

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
