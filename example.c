#include "budoux-c.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  char error[128] = {0};
  struct budouxc *model = NULL;
  struct budouxc_boundaries *boundaries = NULL;

  // Initialize the model with the embedded pre-trained Japanese model.
  model = budouxc_init_embedded_ja(NULL, error);
  if (!model) {
    printf("budouxc_init_embedded_ja failed: %s\n", error);
    goto cleanup;
  }

  static char const sentence[] = "私はその人を常に先生と呼んでいた。\n"
                                 "だからここでもただ先生と書くだけで本名は打ち明けない。\n"
                                 "これは世間を憚かる遠慮というよりも、その方が私にとって自然だからである。";

  // Parse the boundaries of the sentence using the model.
  boundaries = budouxc_parse_boundaries_utf8(model, sentence, strlen(sentence), error);
  if (!boundaries) {
    printf("budouxc_parse_boundaries_utf8 failed: %s\n", error);
    goto cleanup;
  }

  // Print the sentence with "|" characters between each word.
  // The following text will be output.
  //
  // 私は|その人を|常に|先生と|呼んでいた。|
  // だから|ここでも|ただ|先生と|書くだけで|本名は|打ち明けない。|
  // これは|世間を|憚かる|遠慮と|いう|よりも、|その方が|私に|とって|自然だからである。
  size_t pos = 0;
  char buf[256];
  for (size_t i = 0; i < boundaries->n; ++i) {
    size_t const len = boundaries->indices[i] - pos;
    if (len >= sizeof(buf)) {
      printf("buffer too small\n");
      goto cleanup;
    }
    strncpy(buf, sentence + pos, len);
    buf[len] = '\0';
    printf("%s|", buf);
    pos = boundaries->indices[i];
  }
  printf("%s\n", sentence + pos);

cleanup:
  if (boundaries) {
    budouxc_boundaries_destroy(model, boundaries);
  }
  budouxc_destroy(model);
  return 0;
}
