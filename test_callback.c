#include "budoux-c.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static wchar_t const sentence[] = L"私はその人を常に先生と呼んでいた。\n"
                                  L"だからここでもただ先生と書くだけで本名は打ち明けない。\n"
                                  L"これは世間を憚かる遠慮というよりも、その方が私にとって自然だからである。";

static char32_t get_char(struct budouxc_boundaries const *boundaries, void *userdata) {
  (void)boundaries;
  size_t *c = userdata;
  return sentence[(*c)++];
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  char error[128] = {0};
  bool ok = false;
  struct budouxc *model = NULL;
  struct budouxc_boundaries *boundaries_golden = NULL;
  struct budouxc_boundaries *boundaries = NULL;

  // Initialize the model with the embedded pre-trained Japanese model.
  model = budouxc_init_embedded_ja(NULL, error);
  if (!model) {
    printf("budouxc_init_embedded_ja failed: %s\n", error);
    goto cleanup;
  }

  switch (sizeof(wchar_t)) {
  case 2:
    boundaries_golden = budouxc_parse_boundaries_utf16(model, (char16_t const *)sentence, wcslen(sentence), error);
    if (!boundaries_golden) {
      printf("budouxc_parse_boundaries_utf16 failed: %s\n", error);
      goto cleanup;
    }
    break;
  case 4:
    boundaries_golden = budouxc_parse_boundaries_utf32(model, (char32_t const *)sentence, wcslen(sentence), error);
    if (!boundaries_golden) {
      printf("budouxc_parse_boundaries_utf32 failed: %s\n", error);
      goto cleanup;
    }
    break;
  default:
    printf("unsupported wchar_t size\n");
    goto cleanup;
  }

  // Parse the boundaries of the sentence using the model.
  // boundaries = budouxc_parse_boundaries_utf16(model, sentence, u16len(sentence), error);
  boundaries = budouxc_parse_boundaries_callback(model, get_char, &(size_t){0}, error);
  if (!boundaries) {
    printf("budouxc_parse_boundaries_callback failed: %s\n", error);
    goto cleanup;
  }

  // Compare the boundaries with the golden data.
  if (boundaries->n != boundaries_golden->n) {
    printf("number of boundaries mismatch\n");
    goto cleanup;
  }
  for (size_t i = 0; i < boundaries->n; ++i) {
    if (boundaries->indices[i] != boundaries_golden->indices[i]) {
      printf("boundary mismatch at %zu\n", i);
      printf("  expected: %zu, got: %zu\n", boundaries_golden->indices[i], boundaries->indices[i]);
      goto cleanup;
    }
  }
  ok = true;
cleanup:
  if (boundaries) {
    budouxc_boundaries_destroy(model, boundaries);
  }
  if (boundaries_golden) {
    budouxc_boundaries_destroy(model, boundaries_golden);
  }
  budouxc_destroy(model);
  return ok ? 0 : 1;
}
