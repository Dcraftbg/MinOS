#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifndef concat
#define concat(a, b) a ## b
#endif

#ifndef _DSAC_HASHMAP_H
#define _DSAC_HASHMAP_H

#define MAKE_HASHMAP_EX(HashMapT, prefix, T, K, hash_key, key_eq, hash_alloc, hash_dealloc) \
    MAKE_HASHMAP_EX2(HashMapT, prefix, T, K, hash_key, key_eq, hash_alloc, hash_dealloc, hash_alloc, hash_dealloc)

#define _MAKE_HASHMAP_TYPES(HashMapT, prefix, T, K) \
    typedef struct Pair_##HashMapT {\
        struct Pair_##HashMapT *next;\
        K key;\
        T value;\
    } Pair_##HashMapT;\
    typedef struct {\
        struct Pair_##HashMapT *first;\
    } Bucket_##HashMapT;\
    typedef struct {\
        struct {\
            Bucket_##HashMapT *items;\
            size_t len;\
        } buckets;\
        size_t len;\
    } HashMapT;

#define _MAKE_HASHMAP_DECLARATIONS(HashMapT, prefix, T, K, hash_key, key_eq, hash_alloc, hash_dealloc, pair_alloc, pair_dealloc) \
    bool concat(prefix, _resize) (HashMapT* hashmap, size_t extra);\
    bool concat(prefix, _insert) (HashMapT* hashmap, K key, T value);\
    T* concat(prefix, _get) (HashMapT* hashmap, K key);\
    void concat(prefix, _destruct) (HashMapT* hashmap);
#endif // _DSAC_HASHMAP_H


#ifdef MAKE_HASHMAP_EX2
#undef MAKE_HASHMAP_EX2
#endif

#ifndef HASHMAP_DEFINE
#define MAKE_HASHMAP_EX2(HashMapT, prefix, T, K, hash_key, key_eq, hash_alloc, hash_dealloc, pair_alloc, pair_dealloc) \
    _MAKE_HASHMAP_TYPES(HashMapT, prefix, T, K)\
    _MAKE_HASHMAP_DECLARATIONS(HashMapT, prefix, T, K, hash_key, key_eq, hash_alloc, hash_dealloc, pair_alloc, pair_dealloc)
#else
#define MAKE_HASHMAP_EX2(HashMapT, prefix, T, K, hash_key, key_eq, hash_alloc, hash_dealloc, pair_alloc, pair_dealloc) \
    _MAKE_HASHMAP_TYPES(HashMapT, prefix, T, K)\
    _MAKE_HASHMAP_DECLARATIONS(HashMapT, prefix, T, K, hash_key, key_eq, hash_alloc, hash_dealloc, pair_alloc, pair_dealloc)\
    bool concat(prefix, _resize) (HashMapT* hashmap, size_t extra) {\
        if(hashmap->len + extra > hashmap->buckets.len) {\
            size_t ncap = hashmap->buckets.len*2 + extra;\
            concat(Bucket_, HashMapT) *newbucket = hash_alloc(sizeof(*hashmap->buckets.items) * ncap);\
            if(!newbucket) return false;\
            memset(newbucket, 0, sizeof(*hashmap->buckets.items) * ncap);\
            for(size_t i = 0; i < hashmap->buckets.len; ++i) {\
                concat(Bucket_, HashMapT) *oldbucket = &hashmap->buckets.items[i]; \
                while(oldbucket->first) {\
                    size_t hash = hash_key(oldbucket->first->key);\
                    concat(Bucket_, HashMapT) *bucket = &newbucket[hash % ncap];\
                    concat(Pair_, HashMapT) *next = oldbucket->first->next;\
                    if(bucket->first) oldbucket->first->next = bucket->first;\
                    bucket->first = oldbucket->first;\
                    oldbucket->first = next;\
                }\
            }\
            hash_dealloc(hashmap->buckets.items, sizeof(*hashmap->buckets.items) * hashmap->buckets.len);\
            hashmap->buckets.items = newbucket;\
            hashmap->buckets.len = ncap;\
        }\
        return true;\
    }\
    bool concat(prefix, _insert) (HashMapT* hashmap, K key, T value) {\
        if(!concat(prefix, _resize) (hashmap, 1)) return false;\
        concat(Pair_, HashMapT) *pair = pair_alloc(sizeof(concat(Pair_, HashMapT)));\
        if(!pair) return false;\
        pair->next = NULL;\
        pair->key = key;\
        pair->value = value;\
        size_t hash = hash_key(key);\
        concat(Bucket_, HashMapT)* bucket = &hashmap->buckets.items[hash % hashmap->buckets.len];\
        if(bucket->first) pair->next = bucket->first;\
        bucket->first = pair;\
        hashmap->len++;\
        return true;\
    }\
    bool concat(prefix, _remove) (HashMapT* hashmap, K key) {\
        if(!hashmap->buckets.len) return false;\
        size_t hash = hash_key(key);\
        concat(Bucket_, HashMapT)* bucket = &hashmap->buckets.items[hash % hashmap->buckets.len];\
        concat(Pair_, HashMapT)* pair = bucket->first;\
        /* Some dark magic:*/\
        /* Basically because the `next` field will always be at offset 0 it means that we can just take a pointer to the bucket itself*/\
        /* Which simplifies a hell of a lot of stuff*/\
        concat(Pair_, HashMapT)* prev = NULL;\
        while(pair) {\
            if(key_eq(pair->key,key)) {\
                if(prev == NULL) {\
                    bucket->first = NULL;\
                } else {\
                    prev->next = pair->next;\
                }\
                pair_dealloc(pair, sizeof(*pair));\
                hashmap->len--;\
                return true;\
            }\
            prev = pair;\
            pair = pair->next;\
        }\
        return false;\
    }\
    T* concat(prefix, _get) (HashMapT* hashmap, K key) {\
        if(!hashmap->buckets.len) return NULL;\
        size_t hash = hash_key(key);\
        concat(Bucket_, HashMapT)* bucket = &hashmap->buckets.items[hash % hashmap->buckets.len];\
        concat(Pair_, HashMapT)* pair = bucket->first;\
        while(pair) {\
            if(key_eq(pair->key,key)) return &pair->value;\
            pair = pair->next;\
        }\
        return NULL;\
    }\
    void concat(prefix, _destruct) (HashMapT* hashmap) {\
        for(size_t i = 0; i < hashmap->buckets.len; ++i) {\
            concat(Bucket_, HashMapT) *oldbucket = &hashmap->buckets.items[i]; \
            while(oldbucket->first) {\
                concat(Pair_, HashMapT) *next = oldbucket->first->next;\
                pair_dealloc(oldbucket->first, sizeof(*oldbucket->first));\
                oldbucket->first = next;\
            }\
        }\
        hash_dealloc(hashmap->buckets.items, sizeof(*hashmap->buckets.items) * hashmap->buckets.len);\
        hashmap->buckets.items = NULL;\
        hashmap->buckets.len = 0;\
        hashmap->len = 0;\
    }
#endif
