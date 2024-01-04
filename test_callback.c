#include "budoux-c.h"

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static wchar_t const sentence[] = L"私はその人を常に先生と呼んでいた。\n"
                                  L"だからここでもただ先生と書くだけで本名は打ち明けない。\n"
                                  L"これは世間を憚かる遠慮というよりも、その方が私にとって自然だからである。";

struct context {
  size_t nch;
  size_t nb;
  size_t prev_boundary;
  struct budouxc_boundaries *golden;
  bool correct;
};

static void print_word(struct context *const c, size_t const boundary) {
  wchar_t buf[32];
  wcsncpy(buf, sentence + c->prev_boundary, boundary - c->prev_boundary);
  buf[boundary - c->prev_boundary] = L'\0';
  printf("word: %ls\n", buf);
}

static char32_t get_char(void *userdata) {
  struct context *const c = userdata;
  return sentence[c->nch++];
}

static bool add_boundary(size_t const boundary, void *userdata) {
  struct context *const c = userdata;
  size_t const b = c->nb++;
  print_word(c, boundary);
  if (b < c->golden->n && boundary != c->golden->indices[b]) {
    c->correct = false;
    printf("boundary mismatch at %zu\n", b);
    printf("  expected: %zu, got: %zu\n", c->golden->indices[b], boundary);
  }
  c->prev_boundary = boundary;
  return true;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  if (setlocale(LC_ALL, "ja_JP.UTF-8") == NULL) {
    printf("setlocale failed.\n");
    return 1;
  }

  char error[128] = {0};
  bool ok = true;
  struct budouxc *model = NULL;
  struct budouxc_boundaries *boundaries_golden = NULL;

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
      ok = false;
      goto cleanup;
    }
    break;
  case 4:
    boundaries_golden = budouxc_parse_boundaries_utf32(model, (char32_t const *)sentence, wcslen(sentence), error);
    if (!boundaries_golden) {
      printf("budouxc_parse_boundaries_utf32 failed: %s\n", error);
      ok = false;
      goto cleanup;
    }
    break;
  default:
    printf("unsupported wchar_t size\n");
    ok = false;
    goto cleanup;
  }

  // Parse the boundaries of the sentence using the model.
  struct context ctx = {
      .golden = boundaries_golden,
      .correct = true,
  };
  if (!budouxc_parse_boundaries_callback(model, get_char, add_boundary, &ctx)) {
    printf("budouxc_parse_boundaries_callback failed: %s\n", error);
    ok = false;
    goto cleanup;
  }
  print_word(&ctx, wcslen(sentence));
  // Compare the boundaries with the golden data.
  if (ctx.nb != boundaries_golden->n) {
    printf("number of boundaries mismatch\n");
    printf("  expected: %zu, got: %zu\n", boundaries_golden->n, ctx.nb);
    ok = false;
  }
  if (!ctx.correct) {
    ok = false;
  }
cleanup:
  if (boundaries_golden) {
    budouxc_boundaries_destroy(model, boundaries_golden);
  }
  budouxc_destroy(model);
  return ok ? 0 : 1;
}
