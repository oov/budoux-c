#include "budoux-c.h"

#include <stdio.h>
#include <string.h>

static int
check_zero_boundaries_utf32(struct budouxc *const model, char32_t const *const sentence, size_t const sentence_len) {
  char error[128] = {0};
  struct budouxc_boundaries *boundaries = budouxc_parse_boundaries_utf32(model, sentence, sentence_len, error);
  if (!boundaries) {
    printf("budouxc_parse_boundaries_utf32 failed: %s\n", error);
    return 1;
  }
  int failed = 0;
  if (boundaries->n != 0) {
    printf("expected no boundaries but got %zu\n", boundaries->n);
    failed = 1;
  }
  budouxc_boundaries_destroy(model, boundaries);
  return failed;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  char error[128] = {0};
  struct budouxc *model = budouxc_init_embedded_ja(NULL, error);
  if (!model) {
    printf("budouxc_init_embedded_ja failed: %s\n", error);
    return 1;
  }

  int failed = 0;

  // A single code point never yields a boundary.
  char32_t const one_char[] = {0x79c1}; // U+79C1
  failed |= check_zero_boundaries_utf32(model, one_char, 1);

  // The UTF-8 variant with a single (3-byte) code point.
  char const one_char_utf8[] = "\xe7\xa7\x81"; // U+79C1
  struct budouxc_boundaries *boundaries =
      budouxc_parse_boundaries_utf8(model, one_char_utf8, strlen(one_char_utf8), error);
  if (!boundaries) {
    printf("budouxc_parse_boundaries_utf8 failed: %s\n", error);
    failed = 1;
  } else {
    if (boundaries->n != 0) {
      printf("expected no boundaries but got %zu\n", boundaries->n);
      failed = 1;
    }
    budouxc_boundaries_destroy(model, boundaries);
  }

  budouxc_destroy(model);
  return failed;
}
