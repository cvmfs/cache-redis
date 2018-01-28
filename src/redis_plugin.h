#ifndef REDIS_PLUGIN_H_
#define REDIS_PLUGIN_H_

#include <libcvmfs_cache.h>

#include <redox.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <string.h>

struct Object {
  std::string data;
  cvmcache_object_type type;
  int32_t refcnt;
  std::string description;

  std::string Serialize() const;

  static Object Deserialize(const std::string& s);
};

struct TxnInfo {
  struct cvmcache_hash id;
  Object partial_object;
};

class RedisPlugin {
public:
  static int redis_chrefcnt(struct cvmcache_hash *id, int32_t change_by);
  static int redis_obj_info(struct cvmcache_hash *id,
                            struct cvmcache_object_info *info);
  static int redis_pread(struct cvmcache_hash *id, uint64_t offset,
                         uint32_t *size, unsigned char *buffer);
  static int redis_start_txn(struct cvmcache_hash *id, uint64_t txn_id,
                             struct cvmcache_object_info *info);
  static int redis_write_txn(uint64_t txn_id, unsigned char *buffer,
                             uint32_t size);
  static int redis_commit_txn(uint64_t txn_id);
  static int redis_abort_txn(uint64_t txn_id);
  static int redis_info(struct cvmcache_info *info);
  static int redis_shrink(uint64_t shrink_to, uint64_t *used);
  static int redis_listing_begin(uint64_t lst_id,
                                 enum cvmcache_object_type type);
  static int redis_listing_next(int64_t listing_id,
                                struct cvmcache_object_info *item);
  static int redis_listing_end(int64_t listing_id);

  static int Capabilities();

  static void Usage(const char *progname);

  static bool Connect(const std::string& address, int port);

 private:
  static RedisPlugin& Instance();

  RedisPlugin();
  ~RedisPlugin();

  std::map<uint64_t, TxnInfo> transactions_;
  redox::Redox redox_;
};

#endif // REDIS_PLUGIN_H_
