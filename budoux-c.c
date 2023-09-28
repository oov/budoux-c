#include "budoux-c.h"

#include <stdbool.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Unicode related functions ----

static inline size_t first_byte_to_len(int const ch) {
  if (ch <= 0x7f) {
    return 1;
  }
  if ((ch & 0xe0) == 0xc0) {
    return 2;
  }
  if ((ch & 0xf0) == 0xe0) {
    return 3;
  }
  if ((ch & 0xf8) == 0xf0) {
    return 4;
  }
  return 0;
}

static inline bool u8later(int const ch) { return (ch & 0xc0) == 0x80; }

static inline bool invalid_codepoint(char32_t const ch) { return ch > 0x10ffff || (0xd800 <= ch && ch < 0xe000); }

static size_t
utf8to32(char32_t *dest, size_t *byte_indices, size_t const dest_len, char const *const src, size_t const src_len) {
  if (!src || !src_len) {
    return 0;
  }
  char32_t const *const dest_end = dest + dest_len;
  uint8_t const *const u8 = (uint8_t const *)src;
  size_t i = 0;
  while (i < src_len) {
    size_t const ch_len = first_byte_to_len(u8[i]);
    if (!ch_len || i + ch_len > src_len) {
      return 0;
    }
    char32_t codepoint = 0;
    switch (ch_len) {
    case 1:
      codepoint = u8[i];
      break;
    case 2:
      if (!u8later(u8[i + 1])) {
        return 0;
      }
      if (!(u8[i] & 0x1e)) {
        return 0;
      }
      codepoint = ((char32_t)(u8[i] & 0x1f) << 6) | (u8[i + 1] & 0x3f);
      break;
    case 3:
      if (!u8later(u8[i + 1]) || !u8later(u8[i + 2])) {
        return 0;
      }
      if ((u8[i] == 0xe0) && !(u8[i + 1] & 0x20)) {
        return 0;
      }
      codepoint = ((char32_t)(u8[i] & 0x0f) << 12) | ((char32_t)(u8[i + 1] & 0x3f) << 6) | (char32_t)(u8[i + 2] & 0x3f);
      break;
    case 4:
      if (!u8later(u8[i + 1]) || !u8later(u8[i + 2]) || !u8later(u8[i + 3])) {
        return 0;
      }
      if ((u8[i] & 0x07) + (u8[i + 1] & 0x30) == 0) {
        return 0;
      }
      codepoint = ((char32_t)(u8[i] & 0x07) << 18) | ((char32_t)(u8[i + 1] & 0x3f) << 12) |
                  ((char32_t)(u8[i + 2] & 0x3f) << 6) | (char32_t)(u8[i + 3] & 0x3f);
      break;
    }
    if (invalid_codepoint(codepoint)) {
      return 0;
    }
    if (dest_len) {
      if (dest == dest_end) {
        return 0;
      }
      *dest = codepoint;
      if (byte_indices) {
        *byte_indices++ = i;
      }
    }
    ++dest;
    i += ch_len;
  }
  return dest_len ? i : (size_t)(dest - dest_end);
}

// Allocator wrappers ----

static void *hm_realloc(void *ptr, size_t size, void *user_data) {
  struct budouxc_allocators const *const allocators = user_data;
  return allocators->fn_realloc(ptr, size, allocators->user_data);
}

static void hm_free(void *ptr, void *user_data) {
  struct budouxc_allocators const *const allocators = user_data;
  allocators->fn_free(ptr, allocators->user_data);
}

static void *json_alloc(size_t size, int zero, void *user_data) {
  struct budouxc_allocators const *const allocators = user_data;
  void *ptr = allocators->fn_realloc(NULL, size, allocators->user_data);
  if (ptr && zero) {
    memset(ptr, 0, size);
  }
  return ptr;
}

static void json_free(void *ptr, void *user_data) {
  struct budouxc_allocators const *const allocators = user_data;
  allocators->fn_free(ptr, allocators->user_data);
}

static void *realloc_default(void *ptr, size_t size, void *user_data) {
  (void)user_data;
  return realloc(ptr, size);
}
static void free_default(void *ptr, void *user_data) {
  (void)user_data;
  free(ptr);
}

#ifdef __GNUC__
#  ifndef __has_warning
#    define __has_warning(x) 0
#  endif
#  pragma GCC diagnostic push
#  if __has_warning("-Wreserved-macro-identifier")
#    pragma GCC diagnostic ignored "-Wreserved-macro-identifier"
#  endif
#  if __has_warning("-Wreserved-identifier")
#    pragma GCC diagnostic ignored "-Wreserved-identifier"
#  endif
#  if __has_warning("-Wreserved-id-macro")
#    pragma GCC diagnostic ignored "-Wreserved-id-macro"
#  endif
#endif // __GNUC__

#include <json.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif // __GNUC__

#include <hashmap.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define FLOAT_TYPE float

struct unigram {
  char32_t key[1];
  int32_t value;
};

struct bigram {
  char32_t key[2];
  int32_t value;
};

struct trigram {
  char32_t key[3];
  int32_t value;
};

struct budouxc {
  struct budouxc_allocators allocators;
  struct hashmap *uni[6];
  struct hashmap *bi[3];
  struct hashmap *tri[4];
  int32_t sum;
};

#define IMPL_BUILD_MAP(typ)                                                                                            \
  static int typ##_compare(void const *const a, void const *const b, void *const udata) {                              \
    (void)udata;                                                                                                       \
    struct typ const *const aa = a;                                                                                    \
    struct typ const *const bb = b;                                                                                    \
    return memcmp(&aa->key, &bb->key, sizeof(aa->key));                                                                \
  }                                                                                                                    \
  static uint64_t typ##_hash(void const *const item, uint64_t const seed0, uint64_t const seed1, void *const udata) {  \
    (void)udata;                                                                                                       \
    struct typ const *const i = item;                                                                                  \
    return hashmap_sip(&i->key, sizeof(i->key), seed0, seed1);                                                         \
  }                                                                                                                    \
  static struct hashmap *build_##typ##_map(                                                                            \
      json_value const *const obj, struct budouxc_allocators *const allocators, char *const error128) {                \
    struct typ g = {0};                                                                                                \
    struct hashmap *map = NULL;                                                                                        \
    size_t const n = ARRAY_SIZE(g.key);                                                                                \
    map = hashmap_new_with_allocator(                                                                                  \
        hm_realloc, hm_free, sizeof(struct typ), 0, 0, 0, typ##_hash, typ##_compare, NULL, allocators);                \
    for (unsigned int i = 0, len = obj->u.object.length; i < len; ++i) {                                               \
      if (obj->u.object.values[i].value->type != json_integer) {                                                       \
        strcpy(error128, "Invalid JSON structure");                                                                    \
        goto failed;                                                                                                   \
      }                                                                                                                \
      size_t const written =                                                                                           \
          utf8to32(&g.key[0], NULL, n, obj->u.object.values[i].name, obj->u.object.values[i].name_length);             \
      if (written < n) {                                                                                               \
        if (!written) {                                                                                                \
          sprintf(error128, "Failed to convert to codepoint(s): %s", obj->u.object.values[i].name);                    \
          goto failed;                                                                                                 \
        }                                                                                                              \
        memset((&g.key[0]) + written, 0, (n - written) * sizeof(g.key[0]));                                            \
      }                                                                                                                \
      g.value = (int32_t)(obj->u.object.values[i].value->u.integer);                                                   \
      if (hashmap_set(map, &g) == NULL && hashmap_oom(map)) {                                                          \
        strcpy(error128, "Out of memory");                                                                             \
        goto failed;                                                                                                   \
      }                                                                                                                \
    }                                                                                                                  \
    return map;                                                                                                        \
  failed:                                                                                                              \
    hashmap_free(map);                                                                                                 \
    return false;                                                                                                      \
  }                                                                                                                    \
  static struct hashmap *build_##typ##_map(                                                                            \
      json_value const *const obj, struct budouxc_allocators *const allocators, char *const error128)

IMPL_BUILD_MAP(unigram);
IMPL_BUILD_MAP(bigram);
IMPL_BUILD_MAP(trigram);

#undef IMPL_BUILD_MAP

void BUDOUXC_DECLSPEC budouxc_destroy(struct budouxc *const model) {
  if (!model) {
    return;
  }
  for (size_t i = 0; i < ARRAY_SIZE(model->uni); ++i) {
    hashmap_free(model->uni[i]);
    model->uni[i] = NULL;
  }
  for (size_t i = 0; i < ARRAY_SIZE(model->bi); ++i) {
    hashmap_free(model->bi[i]);
    model->bi[i] = NULL;
  }
  for (size_t i = 0; i < ARRAY_SIZE(model->tri); ++i) {
    hashmap_free(model->tri[i]);
    model->tri[i] = NULL;
  }
  model->allocators.fn_free(model, model->allocators.user_data);
}

struct budouxc *BUDOUXC_DECLSPEC budouxc_init(struct budouxc_allocators const *const allocators,
                                              char const *const json,
                                              size_t const json_len,
                                              char *error128) {
  struct budouxc *model = NULL;
  json_value *root = NULL;

  struct budouxc_allocators a = allocators ? *allocators
                                           : (struct budouxc_allocators){
                                                 .fn_realloc = realloc_default,
                                                 .fn_free = free_default,
                                             };
  json_settings settings = {
      .mem_alloc = json_alloc,
      .mem_free = json_free,
      .user_data = &a,
  };

  if (!json || !json_len) {
    strcpy(error128, "Invalid arguments");
    return NULL;
  }

  model = a.fn_realloc(NULL, sizeof(struct budouxc), a.user_data);
  if (!model) {
    strcpy(error128, "Out of memory");
    goto failed;
  }
  *model = (struct budouxc){
      .allocators = a,
  };

  root = json_parse_ex(&settings, json, json_len, error128);
  if (root == NULL) {
    goto failed;
  }
  if (root->type != json_object) {
    strcpy(error128, "JSON root is not an object");
    goto failed;
  }
  // Only UW1 - UW6, BW1 - BW3, TW1 - TW4 are located on the root object.
  for (unsigned int i = 0, len = root->u.object.length; i < len; ++i) {
    char const *const name = root->u.object.values[i].name;
    if (root->u.object.values[i].name_length != 3 || name[1] != 'W') {
      continue;
    }
    int n = name[2] - '1';
    switch (name[0]) {
    case 'U':
      if (n < 0 || n >= (int)(ARRAY_SIZE(model->uni))) {
        continue;
      }
      break;
    case 'B':
      if (n < 0 || n >= (int)(ARRAY_SIZE(model->bi))) {
        continue;
      }
      break;
    case 'T':
      if (n < 0 || n >= (int)(ARRAY_SIZE(model->tri))) {
        continue;
      }
      break;
    default:
      continue;
    }
    json_value const *const obj = root->u.object.values[i].value;
    if (obj->type != json_object) {
      sprintf(error128, "JSON key %s is not an object", root->u.object.values[i].name);
      goto failed;
    }
    switch (name[0]) {
    case 'U':
      if (model->uni[n] != NULL) {
        sprintf(error128, "Duplicate key %s", root->u.object.values[i].name);
        goto failed;
      }
      if ((model->uni[n] = build_unigram_map(obj, &model->allocators, error128)) == NULL) {
        goto failed;
      }
      break;
    case 'B':
      if (model->bi[n] != NULL) {
        sprintf(error128, "Duplicate key %s", root->u.object.values[i].name);
        goto failed;
      }
      if ((model->bi[n] = build_bigram_map(obj, &model->allocators, error128)) == NULL) {
        goto failed;
      }
      break;
    case 'T':
      if (model->tri[n] != NULL) {
        sprintf(error128, "Duplicate key %s", root->u.object.values[i].name);
        goto failed;
      }
      if ((model->tri[n] = build_trigram_map(obj, &model->allocators, error128)) == NULL) {
        goto failed;
      }
      break;
    }
  }

#define SUM_MAP(typ, map, sum)                                                                                         \
  do {                                                                                                                 \
    size_t iter = 0;                                                                                                   \
    void *item = NULL;                                                                                                 \
    while (hashmap_iter(map, &iter, &item)) {                                                                          \
      struct typ const *const j = item;                                                                                \
      sum += j->value;                                                                                                 \
    }                                                                                                                  \
  } while (0)

  int32_t sum = 0;
  for (size_t i = 0; i < ARRAY_SIZE(model->uni); ++i) {
    if (!model->uni[i]) {
      sprintf(error128, "Missing key UW%zu", i + 1);
      goto failed;
    }
    SUM_MAP(unigram, model->uni[i], sum);
  }
  for (size_t i = 0; i < ARRAY_SIZE(model->bi); ++i) {
    if (!model->bi[i]) {
      sprintf(error128, "Missing key BW%zu", i + 1);
      goto failed;
    }
    SUM_MAP(bigram, model->bi[i], sum);
  }
  for (size_t i = 0; i < ARRAY_SIZE(model->tri); ++i) {
    if (!model->tri[i]) {
      sprintf(error128, "Missing key TW%zu", i + 1);
      goto failed;
    }
    SUM_MAP(trigram, model->tri[i], sum);
  }
#undef SUM_MAP

  model->sum = sum;
  json_value_free_ex(&settings, root);
  return model;
failed:
  if (root != NULL) {
    json_value_free_ex(&settings, root);
  }
  if (model) {
    budouxc_destroy(model);
  }
  return NULL;
}

static inline void get_unigram_score(int32_t *score, struct hashmap *const map, char32_t const k0) {
  struct unigram const *const item = hashmap_get(map, &(struct unigram const){.key = {k0}});
  if (item) {
    *score += item->value;
  }
}

static inline void get_bigram_score(int32_t *score, struct hashmap *const map, char32_t const k0, char32_t const k1) {
  struct bigram const *const item = hashmap_get(map, &(struct bigram const){.key = {k0, k1}});
  if (item) {
    *score += item->value;
  }
}

static inline void
get_trigram_score(int32_t *score, struct hashmap *const map, char32_t const k0, char32_t const k1, char32_t const k2) {
  struct trigram const *const item = hashmap_get(map, &(struct trigram const){.key = {k0, k1, k2}});
  if (item) {
    *score += item->value;
  }
}

struct budouxc_boundaries *BUDOUXC_DECLSPEC budouxc_parse_boundaries_utf32(struct budouxc *const model,
                                                                           char32_t const *const sentence,
                                                                           size_t const sentence_len,
                                                                           char *error128) {
  size_t *r = NULL;
  size_t r_len = 0;
  size_t r_cap = 0;

  FLOAT_TYPE const base_score = (FLOAT_TYPE)(model->sum) * (FLOAT_TYPE)(-0.5);
  for (size_t i = 1; i < sentence_len; ++i) {
    int32_t score = 0;
    if (i >= 3) {
      get_unigram_score(&score, model->uni[0], sentence[i - 3]);
    }
    if (i >= 2) {
      get_unigram_score(&score, model->uni[1], sentence[i - 2]);
    }
    get_unigram_score(&score, model->uni[2], sentence[i - 1]);
    get_unigram_score(&score, model->uni[3], sentence[i]);
    if (i + 1 < sentence_len) {
      get_unigram_score(&score, model->uni[4], sentence[i + 1]);
    }
    if (i + 2 < sentence_len) {
      get_unigram_score(&score, model->uni[5], sentence[i + 2]);
    }

    if (i >= 2) {
      get_bigram_score(&score, model->bi[0], sentence[i - 2], sentence[i - 1]);
    }
    get_bigram_score(&score, model->bi[1], sentence[i - 1], sentence[i]);
    if (i + 1 < sentence_len) {
      get_bigram_score(&score, model->bi[2], sentence[i], sentence[i + 1]);
    }

    if (i >= 3) {
      get_trigram_score(&score, model->tri[0], sentence[i - 3], sentence[i - 2], sentence[i - 1]);
    }
    if (i >= 2) {
      get_trigram_score(&score, model->tri[1], sentence[i - 2], sentence[i - 1], sentence[i]);
    }
    if (i + 1 < sentence_len) {
      get_trigram_score(&score, model->tri[2], sentence[i - 1], sentence[i], sentence[i + 1]);
    }
    if (i + 2 < sentence_len) {
      get_trigram_score(&score, model->tri[3], sentence[i], sentence[i + 1], sentence[i + 2]);
    }

    if (base_score + (FLOAT_TYPE)(score) > 0) {
      if (r_len == r_cap) {
        size_t const newcap = r_cap ? r_cap * 2 : 16;
        size_t *newbuf = model->allocators.fn_realloc(
            r, newcap * sizeof(size_t) + sizeof(struct budouxc_boundaries), model->allocators.user_data);
        if (!newbuf) {
          strcpy(error128, "Out of memory");
          goto failed;
        }
        r = newbuf;
        r_cap = newcap;
      }
      r[r_len++] = i;
    }
  }
  struct budouxc_boundaries *ret = (void *)(r + r_cap);
  ret->indices = r;
  ret->n = r_len;
  return ret;
failed:
  if (r) {
    model->allocators.fn_free(r, model->allocators.user_data);
  }
  return NULL;
}

struct budouxc_boundaries *BUDOUXC_DECLSPEC budouxc_parse_boundaries_utf8(struct budouxc *const model,
                                                                          char const *const sentence,
                                                                          size_t const sentence_len,
                                                                          char *error128) {
  char32_t *temp = NULL;
  struct budouxc_boundaries *boundaries = NULL;
  size_t const u32chars = utf8to32(NULL, NULL, 0, sentence, sentence_len);
  if (!u32chars) {
    strcpy(error128, "Broken input");
    goto failed;
  }
  temp =
      model->allocators.fn_realloc(NULL, u32chars * (sizeof(char32_t) + sizeof(size_t)), model->allocators.user_data);
  if (!temp) {
    strcpy(error128, "Out of memory");
    goto failed;
  }
  size_t *const utf8_indices = (void *)(temp + u32chars);
  if (!utf8to32(temp, utf8_indices, u32chars, sentence, sentence_len)) {
    strcpy(error128, "Broken input");
    goto failed;
  }
  boundaries = budouxc_parse_boundaries_utf32(model, temp, u32chars, error128);
  if (!boundaries) {
    goto failed;
  }
  // Convert UTF-32 indices to UTF-8 indices.
  for (size_t i = 0, len = boundaries->n; i < len; ++i) {
    boundaries->indices[i] = utf8_indices[boundaries->indices[i]];
  }
  model->allocators.fn_free(temp, model->allocators.user_data);
  return boundaries;
failed:
  if (boundaries) {
    budouxc_boundaries_destroy(model, boundaries);
  }
  if (temp) {
    model->allocators.fn_free(temp, model->allocators.user_data);
  }
  return NULL;
}

void BUDOUXC_DECLSPEC budouxc_boundaries_destroy(struct budouxc *const model,
                                                 struct budouxc_boundaries *const boundaries) {
  if (!model || !boundaries) {
    return;
  }
  model->allocators.fn_free(boundaries->indices, model->allocators.user_data);
}

#ifndef BUDOUXC_NO_EMBEDDED_MODELS

struct budouxc *BUDOUXC_DECLSPEC budouxc_init_embedded_ja(struct budouxc_allocators const *const allocators,
                                                          char *error128) {
  extern unsigned char ja_json[];
  extern unsigned int ja_json_len;
  return budouxc_init(allocators, (char const *)ja_json, (size_t)ja_json_len, error128);
}

struct budouxc *BUDOUXC_DECLSPEC budouxc_init_embedded_zh_hans(struct budouxc_allocators const *const allocators,
                                                               char *error128) {
  extern unsigned char zh_hans_json[];
  extern unsigned int zh_hans_json_len;
  return budouxc_init(allocators, (char const *)zh_hans_json, (size_t)zh_hans_json_len, error128);
}

struct budouxc *BUDOUXC_DECLSPEC budouxc_init_embedded_zh_hant(struct budouxc_allocators const *const allocators,
                                                               char *error128) {
  extern unsigned char zh_hant_json[];
  extern unsigned int zh_hant_json_len;
  return budouxc_init(allocators, (char const *)zh_hant_json, (size_t)zh_hant_json_len, error128);
}

#endif // BUDOUXC_NO_EMBEDDED_MODELS
