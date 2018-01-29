#include "redis_plugin.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <cstring>

#include "util.h"

namespace {

std::string IdToString(const struct cvmcache_hash& id) {
  auto s = std::string(reinterpret_cast<const char*>(id.digest), 20);
  return Base64(s);
}

bool Exists(redox::Redox& rdx, const struct cvmcache_hash& id) {
  const auto digest = IdToString(id);

  auto& c = rdx.commandSync<int>({"EXISTS", digest});
  bool ret = false;
  if(c.ok()) {
    ret = c.reply() > 0;
  }
  c.free();

  return ret;
}

Object Get(redox::Redox& rdx, const struct cvmcache_hash& id) {
  const auto digest = IdToString(id);
  std::string val;
  Debase64(rdx.get(digest), &val);

  return Object::Deserialize(val);
}

void Set(redox::Redox& rdx, const struct cvmcache_hash& id, const Object& obj) {
  const auto val = Base64(obj.Serialize());
  const auto digest = IdToString(id);

  rdx.set(digest, val);
}

}

std::string Object::Serialize() const {
  const size_t data_size = this->data.size();
  const size_t desc_size = this->description.size();

  // data_size + data + type + refcnt + description_size + description
  const auto len = sizeof(data_size) + this->data.size() +
      sizeof(cvmcache_object_type) + sizeof(this->refcnt) + sizeof(data_size) +
      this->description.size();

  std::vector<char> out(len);
  auto dest = out.data();

  std::memcpy(dest, &data_size, sizeof(data_size));
  dest += sizeof(data_size);

  std::memcpy(dest, this->data.data(), data_size);
  dest += data_size;

  std::memcpy(dest, &this->type, sizeof(cvmcache_object_type));
  dest += sizeof(cvmcache_object_type);

  std::memcpy(dest, &this->refcnt, sizeof(this->refcnt));
  dest += sizeof(this->refcnt);

  std::memcpy(dest, &desc_size, sizeof(desc_size));
  dest += sizeof(desc_size);

  std::memcpy(dest, this->description.data(), desc_size);
  return std::string(out.data(), out.size());
}

Object Object::Deserialize(const std::string& s) {
  size_t data_size = 0;
  size_t desc_size = 0;

  Object obj;

  auto src = s.data();

  std::memcpy(&data_size, src, sizeof(data_size));
  src += sizeof(data_size);

  std::vector<char> buf(data_size);
  std::memcpy(buf.data(), src, data_size);
  obj.data = std::string(buf.data(), buf.size());
  src += data_size;

  std::memcpy(&obj.type, src, sizeof(cvmcache_object_type));
  src += sizeof(cvmcache_object_type);

  std::memcpy(&obj.refcnt, src, sizeof(obj.refcnt));
  src += sizeof(obj.refcnt);

  std::memcpy(&desc_size, src, sizeof(desc_size));
  buf.resize(desc_size);
  src += sizeof(desc_size);

  std::memcpy(buf.data(), src, desc_size);
  obj.description = std::string(buf.data(), buf.size());

  return obj;
}

int RedisPlugin::redis_chrefcnt(struct cvmcache_hash *h, int32_t change_by) {
  auto& self = Instance();

  if (!Exists(self.redox_, *h)) {
    return CVMCACHE_STATUS_NOENTRY;
  }

  Object obj = Get(self.redox_, *h);
  obj.refcnt += change_by;
  if (obj.refcnt < 0)
    return CVMCACHE_STATUS_BADCOUNT;
  Set(self.redox_, *h, obj);
  return CVMCACHE_STATUS_OK;
}

int RedisPlugin::redis_obj_info(struct cvmcache_hash *h,
                                struct cvmcache_object_info *info) {
  auto& self = Instance();

  if (!Exists(self.redox_, *h)) {
    return CVMCACHE_STATUS_NOENTRY;
  }

  Object obj = Get(self.redox_, *h);
  info->size = obj.data.length();
  info->type = obj.type;
  info->pinned = obj.refcnt > 0;
  info->description = strdup(obj.description.c_str());
  return CVMCACHE_STATUS_OK;
}

int RedisPlugin::redis_pread(struct cvmcache_hash *h, uint64_t offset,
                             uint32_t *size, unsigned char *buffer) {
  auto& self = Instance();

  const Object obj = Get(self.redox_, *h);
  std::string data = obj.data;
  if (offset > data.length()) {
    return CVMCACHE_STATUS_OUTOFBOUNDS;
  }
  unsigned nbytes =
      std::min(*size, static_cast<uint32_t>(data.length() - offset));
  memcpy(buffer, data.data() + offset, nbytes);
  *size = nbytes;
  return CVMCACHE_STATUS_OK;
}

int RedisPlugin::redis_start_txn(struct cvmcache_hash *h, uint64_t txn_id,
                                 struct cvmcache_object_info *info) {
  auto& self = Instance();

  Object partial_object;
  partial_object.type = info->type;
  partial_object.refcnt = 1;
  if (info->description != NULL) {
    partial_object.description = std::string(info->description);
  }
  TxnInfo txn;
  txn.id = *h;
  txn.partial_object = partial_object;
  self.transactions_[txn_id] = txn;
  return CVMCACHE_STATUS_OK;
}

int RedisPlugin::redis_write_txn(uint64_t txn_id, unsigned char *buffer,
                                 uint32_t size) {
  auto& self = Instance();

  TxnInfo txn = self.transactions_[txn_id];
  txn.partial_object.data +=
      std::string(reinterpret_cast<char *>(buffer), size);
  self.transactions_[txn_id] = txn;
  return CVMCACHE_STATUS_OK;
}

int RedisPlugin::redis_commit_txn(uint64_t txn_id) {
  auto& self = Instance();

  TxnInfo txn = self.transactions_[txn_id];
  Set(self.redox_, txn.id, txn.partial_object);
  self.transactions_.erase(txn_id);
  return CVMCACHE_STATUS_OK;
}

int RedisPlugin::redis_abort_txn(uint64_t txn_id) {
  auto& self = Instance();

  self.transactions_.erase(txn_id);
  return CVMCACHE_STATUS_OK;
}
int RedisPlugin::Capabilities() {
  return CVMCACHE_CAP_WRITE | CVMCACHE_CAP_REFCOUNT;
}

void RedisPlugin::Usage(const char *progname) {
  printf("%s <config file>\n", progname);
}

bool RedisPlugin::Connect(const std::string& address, int port) {
  return Instance().redox_.connect(address, port);
}

RedisPlugin &RedisPlugin::Instance() {
  static RedisPlugin instance;
  return instance;
}

RedisPlugin::RedisPlugin() : transactions_(), redox_() {}

RedisPlugin::~RedisPlugin() {
  redox_.disconnect();
}


