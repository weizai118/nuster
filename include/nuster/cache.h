/*
 * include/nuster/cache.h
 * This file defines everything related to nuster cache.
 *
 * Copyright (C) [Jiang Wenyuan](https://github.com/jiangwenyuan), < koubunen AT gmail DOT com >
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, version 2.1
 * exclusively.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _NUSTER_CACHE_H
#define _NUSTER_CACHE_H

#include <common/config.h>
#include <common/memory.h>

#include <types/global.h>
#include <types/acl.h>
#include <types/filters.h>
#include <types/applet.h>

#include <nuster/memory.h>

#define NST_CACHE_DEFAULT_SIZE                1024 * 1024
#define NST_CACHE_DEFAULT_TTL                 3600
#define NST_CACHE_DEFAULT_DICT_SIZE           32
#define NST_CACHE_DEFAULT_LOAD_FACTOR         0.75
#define NST_CACHE_DEFAULT_GROWTH_FACTOR       2
#define NST_CACHE_DEFAULT_KEY                "method.scheme.host.uri"
#define NST_CACHE_DEFAULT_CODE               "200"
#define NST_CACHE_DEFAULT_KEY_SIZE            128
#define NST_CACHE_DEFAULT_CHUNK_SIZE          32
#define NST_CACHE_DEFAULT_PURGE_METHOD       "PURGE"
#define NST_CACHE_DEFAULT_PURGE_METHOD_SIZE   16

enum {
    NST_CACHE_STATUS_UNDEFINED = -1,
    NST_CACHE_STATUS_OFF       =  0,
    NST_CACHE_STATUS_ON        =  1,
};
enum {
    NST_CACHE_SHARE_UNDEFINED  = -1,
    NST_CACHE_SHARE_OFF        =  0,
    NST_CACHE_SHARE_ON         =  1,
};

enum nst_cache_key_type {
    NST_CACHE_KEY_METHOD = 1,                /* method:    GET, POST... */
    NST_CACHE_KEY_SCHEME,                    /* scheme:    http, https */
    NST_CACHE_KEY_HOST,                      /* host:      Host header   */
    NST_CACHE_KEY_URI,                       /* uri:       first slash to end of the url */
    NST_CACHE_KEY_PATH,                      /* path:      first slach to question mark */
    NST_CACHE_KEY_DELIMITER,                 /* delimiter: '?' or '' */
    NST_CACHE_KEY_QUERY,                     /* query:     question mark to end of the url, or empty */
    NST_CACHE_KEY_PARAM,                     /* param:     query key/value pair */
    NST_CACHE_KEY_HEADER,                    /* header */
    NST_CACHE_KEY_COOKIE,                    /* cookie */
    NST_CACHE_KEY_BODY,                      /* body   */
};

struct nst_cache_key {
    enum nst_cache_key_type  type;
    char                    *data;
};

struct nst_cache_code {
    struct nst_cache_code *next;
    int                    code;
};

enum {
    NST_CACHE_RULE_DISABLED = 0,
    NST_CACHE_RULE_ENABLED  = 1,
};

struct nst_cache_rule {
    struct list             list;       /* list linked to from the proxy */
    struct acl_cond        *cond;       /* acl condition to meet */
    char                   *name;       /* cache name for logging */
    struct nst_cache_key  **key;        /* key */
    struct nst_cache_code  *code;       /* code */
    uint32_t               *ttl;        /* ttl: seconds, 0: not expire */
    int                    *state;      /* on when start, can be turned off by manager API */
    int                     id;         /* same for identical names */
    int                     uuid;       /* unique cache-rule ID */
};

struct nst_cache_element {
    struct nst_cache_element *next;
    char                     *msg;
    int                       msg_len;
};

/*
 * A nst_cache_data contains a complete http response data,
 * and is pointed by nst_cache_entry->data.
 * All nst_cache_data are stored in a circular singly linked list
 */
struct nst_cache_data {
    int                       clients;
    int                       invalid;
    struct nst_cache_element *element;
    struct nst_cache_data    *next;
};

/*
 * A nst_cache_entry is an entry in nst_cache_dict hash table
 */
enum {
    NST_CACHE_ENTRY_STATE_CREATING = 0,
    NST_CACHE_ENTRY_STATE_VALID    = 1,
    NST_CACHE_ENTRY_STATE_INVALID  = 2,
    NST_CACHE_ENTRY_STATE_EXPIRED  = 3,
};

struct nst_cache_entry {
    int                     state;
    char                   *key;
    uint64_t                hash;
    struct nst_cache_data  *data;
    uint64_t                expire;
    uint64_t                atime;
    struct nuster_str       host;
    struct nuster_str       path;
    struct nst_cache_entry *next;
    struct nst_cache_rule  *rule;        /* rule */
    int                     pid;         /* proxy uuid */
};

struct nst_cache_dict {
    struct nst_cache_entry **entry;
    uint64_t                 size;      /* number of entries */
    uint64_t                 used;      /* number of used entries */
#if defined NUSTER_USE_PTHREAD || defined USE_PTHREAD_PSHARED
    pthread_mutex_t          mutex;
#else
    unsigned int             waiters;
#endif
};

struct nst_cache_rule_stash {
    struct nst_cache_rule_stash *next;
    struct nst_cache_rule       *rule;
    char                        *key;
    uint64_t                     hash;
};

enum {
    NST_CACHE_CTX_STATE_INIT   = 0,   /* init */
    NST_CACHE_CTX_STATE_CREATE = 1,   /* to cache */
    NST_CACHE_CTX_STATE_DONE   = 2,   /* cache done */
    NST_CACHE_CTX_STATE_BYPASS = 3,   /* not cached, return to regular process */
    NST_CACHE_CTX_STATE_WAIT   = 4,   /* caching, wait */
    NST_CACHE_CTX_STATE_HIT    = 5,   /* cached, use cache */
    NST_CACHE_CTX_STATE_PASS   = 6,   /* cache rule passed */
    NST_CACHE_CTX_STATE_FULL   = 7,   /* cache full */
};

struct nst_cache_ctx {
    int                          state;

    struct nst_cache_rule       *rule;
    struct nst_cache_rule_stash *stash;

    struct nst_cache_entry      *entry;
    struct nst_cache_data       *data;
    struct nst_cache_element    *element;

    struct {
        int                      scheme;
        struct nuster_str        host;
        struct nuster_str        uri;
        struct nuster_str        path;
        int                      delimiter;
        struct nuster_str        query;
        struct nuster_str        cookie;
    } req;
    int                          pid;         /* proxy uuid */
};

struct nst_cache_stats {
    uint64_t        used_mem;

    struct {
        uint64_t    total;
        uint64_t    fetch;
        uint64_t    hit;
        uint64_t    abort;
    } request;
#if defined NUSTER_USE_PTHREAD || defined USE_PTHREAD_PSHARED
    pthread_mutex_t mutex;
#else
    unsigned int    waiters;
#endif
};

struct cache {
    struct nst_cache_dict  dict[2];           /* 0: using, 1: rehashing */
    struct nst_cache_data *data_head;         /* point to the circular linked list, tail->next ===  head */
    struct nst_cache_data *data_tail;         /* and will be moved together constantly to check invalid data */
#if defined NUSTER_USE_PTHREAD || defined USE_PTHREAD_PSHARED
    pthread_mutex_t        mutex;
#else
    unsigned int           waiters;
#endif

    int                    rehash_idx;        /* >=0: rehashing, index, -1: not rehashing */
    int                    cleanup_idx;       /* cache dict cleanup index */
};

struct cache_config {
    int status;
};


extern struct cache   *cache;
extern struct applet   cache_io_applet;
extern struct applet   cache_manager_applet;
extern struct applet   cache_stats_applet;
extern struct flt_ops  cache_filter_ops;

enum {
    NUSTER_CACHE_200 = 0,
    NUSTER_CACHE_400,
    NUSTER_CACHE_404,
    NUSTER_CACHE_500,
    NUSTER_CACHE_MSG_SIZE
};

enum {
    NST_CACHE_PURGE_NAME_ALL = 0,
    NST_CACHE_PURGE_NAME_PROXY,
    NST_CACHE_PURGE_NAME_RULE,
    NST_CACHE_PURGE_PATH,
    NST_CACHE_PURGE_REGEX,
    NST_CACHE_PURGE_HOST,
    NST_CACHE_PURGE_PATH_HOST,
    NST_CACHE_PURGE_REGEX_HOST,
};

enum {
    NST_CACHE_STATS_HEAD,
    NST_CACHE_STATS_DATA,
    NST_CACHE_STATS_DONE,
};


/* dict */
int nst_cache_dict_init();
struct nst_cache_entry *nst_cache_dict_get(const char *key, uint64_t hash);
struct nst_cache_entry *nst_cache_dict_set(const char *key, uint64_t hash, struct nst_cache_ctx *ctx);
void nst_cache_dict_rehash();
void nst_cache_dict_cleanup();


/* engine */
void nst_cache_init();
void nst_cache_housekeeping();
int nst_cache_prebuild_key(struct nst_cache_ctx *ctx, struct stream *s, struct http_msg *msg);
char *nst_cache_build_key(struct nst_cache_ctx *ctx, struct nst_cache_key **pck, struct stream *s,
        struct http_msg *msg);
char *nst_cache_build_purge_key(struct stream *s, struct http_msg *msg);
uint64_t nst_cache_hash_key(const char *key);
void nst_cache_create(struct nst_cache_ctx *ctx, char *key, uint64_t hash);
int nst_cache_update(struct nst_cache_ctx *ctx, struct http_msg *msg, long msg_len);
void nst_cache_finish(struct nst_cache_ctx *ctx);
void nst_cache_abort(struct nst_cache_ctx *ctx);
struct nst_cache_data *nst_cache_exists(const char *key, uint64_t hash);
struct nst_cache_data *nst_cache_data_new();
void nst_cache_hit(struct stream *s, struct stream_interface *si,
        struct channel *req, struct channel *res, struct nst_cache_data *data);
struct nst_cache_rule_stash *nst_cache_stash_rule(struct nst_cache_ctx *ctx,
        struct nst_cache_rule *rule, char *key, uint64_t hash);
int nst_cache_test_rule(struct nst_cache_rule *rule, struct stream *s, int res);

/* manager */
int nst_cache_purge(struct stream *s, struct channel *req, struct proxy *px);
int cache_manager(struct stream *s, struct channel *req, struct proxy *px);


/* parser */
int cache_parse_filter(char **args, int *cur_arg, struct proxy *px,
        struct flt_conf *fconf, char **err, void *private);
int cache_parse_rule(char **args, int section, struct proxy *proxy,
        struct proxy *defpx, const char *file, int line, char **err);
const char *cache_parse_size(const char *text, uint64_t *ret);
const char *cache_parse_time(const char *text, int len, unsigned *ret);

/* cache memory */
static inline void *cache_memory_alloc(struct pool_head *pool, int size) {
    if(global.cache.share) {
        return nuster_memory_alloc(global.cache.memory, size);
    } else {
        return pool_alloc2(pool);
    }
}
static inline void cache_memory_free(struct pool_head *pool, void *p) {
    if(global.cache.share) {
        return nuster_memory_free(global.cache.memory, p);
    } else {
        return pool_free2(pool, p);
    }
}

/* stats */
void cache_stats_update_used_mem(int i);
int cache_stats_init();
int cache_stats_full();
int cache_stats(struct stream *s, struct channel *req, struct proxy *px);
void cache_stats_update_request(int state);

static inline int cache_check_uri(struct http_msg *msg) {
    const char *uri = msg->chn->buf->p + msg->sl.rq.u;

    if(!global.cache.uri) {
        return 0;
    }

    if(strlen(global.cache.uri) != msg->sl.rq.u_l) {
        return 0;
    }

    if(memcmp(uri, global.cache.uri, msg->sl.rq.u_l) != 0) {
        return 0;
    }

    return 1;
}


#endif /* _NUSTER_CACHE_H */