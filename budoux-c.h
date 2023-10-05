#pragma once

#include <stddef.h>
#include <uchar.h>

#if (defined(_WIN32) || defined(WIN32)) && defined(BUDOUXC_SHARED)
#  ifdef BUDOUXC_EXPORT
#    define BUDOUXC_DECLSPEC __declspec(dllexport)
#  else
#    define BUDOUXC_DECLSPEC __declspec(dllimport)
#  endif
#else
#  define BUDOUXC_DECLSPEC
#endif

/**
 * @brief Struct containing memory allocation functions to be used by budoux.
 */
struct budouxc_allocators {
  void *(*fn_realloc)(void *ptr, size_t size, void *user_data);
  void (*fn_free)(void *ptr, void *user_data);
  void *user_data;
};

/**
 * @brief Initializes a budoux model with the given JSON.
 *
 * @param allocators Pointer to the struct containing the memory allocation functions to be used. If NULL, default
 * implementation will be used.
 * @param json Pointer to the JSON string.
 * @param json_len Length of the JSON string.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to the initialized budoux model, or NULL if initialization failed.
 */
struct budouxc *BUDOUXC_DECLSPEC budouxc_init(struct budouxc_allocators const *const allocators,
                                              char const *const json,
                                              size_t const json_len,
                                              char *error128);

#ifndef BUDOUXC_NO_EMBEDDED_MODELS

/**
 * @brief Initializes a budoux model with the embedded Japanese model.
 *
 * @param allocators Pointer to the struct containing the memory allocation functions to be used. If NULL, default
 * implementation will be used.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to the initialized budoux model, or NULL if initialization failed.
 *
 * @see budouxc_init
 */
struct budouxc *BUDOUXC_DECLSPEC budouxc_init_embedded_ja(struct budouxc_allocators const *const allocators,
                                                          char *error128);

/**
 * @brief Initializes a budoux model with the embedded Simplified Chinese model.
 *
 * @param allocators Pointer to the struct containing the memory allocation functions to be used. If NULL, default
 * implementation will be used.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to the initialized budoux model, or NULL if initialization failed.
 *
 * @see budouxc_init
 */
struct budouxc *BUDOUXC_DECLSPEC budouxc_init_embedded_zh_hans(struct budouxc_allocators const *const allocators,
                                                               char *error128);

/**
 * @brief Initializes a budoux model with the embedded Traditional Chinese model.
 *
 * @param allocators Pointer to the struct containing the memory allocation functions to be used. If NULL, default
 * implementation will be used.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to the initialized budoux model, or NULL if initialization failed.
 *
 * @see budouxc_init
 */
struct budouxc *BUDOUXC_DECLSPEC budouxc_init_embedded_zh_hant(struct budouxc_allocators const *const allocators,
                                                               char *error128);

#endif // BUDOUXC_NO_EMBEDDED_MODELS

/**
 * @brief Destroys a budoux model and frees all associated memory.
 *
 * @param model Pointer to the budoux model to be destroyed.
 */
void BUDOUXC_DECLSPEC budouxc_destroy(struct budouxc *const model);

/**
 * @brief Structure representing the boundaries of a string segmentation
 *
 * indices: an array storing the indices of the segmentation boundaries
 * n: the number of segmentation boundaries
 */
struct budouxc_boundaries {
  size_t *indices;
  size_t n;
};

/**
 * @brief Parses a sentence and returns the word boundaries.
 *
 * @param model Pointer to the budoux model to be used for parsing.
 * @param sentence Pointer to the sentence to be parsed, as an array of UTF-32 code points.
 * @param sentence_len Length of the sentence in code points.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to a struct containing an array of indices into the sentence, representing the word boundaries. The
 * struct is owned by the caller and must be freed with `budouxc_boundaries_destroy`.
 *
 * @see budouxc_boundaries_destroy
 */
struct budouxc_boundaries *BUDOUXC_DECLSPEC budouxc_parse_boundaries_utf32(struct budouxc *const model,
                                                                           char32_t const *const sentence,
                                                                           size_t const sentence_len,
                                                                           char *error128);

/**
 * @brief Parses a sentence and returns the word boundaries.
 *
 * @param model Pointer to the budoux model to be used for parsing.
 * @param sentence Pointer to the sentence to be parsed, as an array of UTF-16 code points.
 * @param sentence_len Length of the sentence in code points.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to a struct containing an array of indices into the sentence, representing the word boundaries. The
 * struct is owned by the caller and must be freed with `budouxc_boundaries_destroy`.
 *
 * @see budouxc_boundaries_destroy
 */
struct budouxc_boundaries *BUDOUXC_DECLSPEC budouxc_parse_boundaries_utf16(struct budouxc *const model,
                                                                           char16_t const *const sentence,
                                                                           size_t const sentence_len,
                                                                           char *error128);

/**
 * @brief Parses a sentence and returns the word boundaries.
 *
 * @param model Pointer to the budoux model to be used for parsing.
 * @param sentence Pointer to the sentence to be parsed, as a UTF-8 string.
 * @param sentence_len Length of the sentence in bytes.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to a struct containing an array of indices into the sentence, representing the word boundaries. The
 * struct is owned by the caller and must be freed with `budouxc_boundaries_destroy`.
 *
 * @see budouxc_boundaries_destroy
 */
struct budouxc_boundaries *BUDOUXC_DECLSPEC budouxc_parse_boundaries_utf8(struct budouxc *const model,
                                                                          char const *const sentence,
                                                                          size_t const sentence_len,
                                                                          char *error128);

/**
 * @brief Parses a sentence and returns the word boundaries.
 *
 * @param model Pointer to the budoux model to be used for parsing.
 * @param get_char Callback function that gets a character from the input sentence and returns 0 at the end.
 * @param userdata Pointer to user-defined data that will be passed to the get_char callback.
 * @param error128 Pointer to a buffer of at least 128 bytes to store error messages in case of failure.
 * @return Pointer to a struct containing an array of indices into the sentence, representing the word boundaries. The
 * struct is owned by the caller and must be freed with `budouxc_boundaries_destroy`.
 *
 * @see budouxc_boundaries_destroy
 */
struct budouxc_boundaries *BUDOUXC_DECLSPEC budouxc_parse_boundaries_callback(struct budouxc *const model,
                                                                              char32_t (*get_char)(size_t const index,
                                                                                                   void *userdata),
                                                                              void *userdata,
                                                                              char *error128);

/**
 * @brief Frees an array of word boundaries returned by `budouxc_parse_boundaries_utf8` or
 * `budouxc_parse_boundaries_utf32`.
 *
 * @param model Pointer to the budoux model that was used for parsing.
 * @param boundaries Pointer to the struct containing the array of word boundaries to be freed.
 *
 * @see budouxc_parse_boundaries_utf8
 * @see budouxc_parse_boundaries_utf32
 */
void BUDOUXC_DECLSPEC budouxc_boundaries_destroy(struct budouxc *const model,
                                                 struct budouxc_boundaries *const boundaries);
