#include "common.h"
#include <stdlib.h>
#include <string.h>

static bool iswhitespace(const char c) {
  return (c == ' ' || c == '\t' || c == '\v');
}

char *stripstring(const char *input) {
  if (input == NULL) {
    return NULL;
  }

  // Skip leading whitespace
  while (*input && iswhitespace(*input)) {
    input++;
  }

  // If string is empty or only whitespace, return empty string
  if (*input == '\0') {
    char *empty = malloc(1);
    if (empty) {
      *empty = '\0';
    }
    return empty;
  }

  // Find the end of the string
  size_t len = strlen(input);
  const char *end = input + len - 1;

  // Move backwards to find the last non-whitespace character
  while (end > input && iswhitespace(*end)) {
    end--;
  }

  // Calculate the trimmed length (end points to the last character to keep)
  size_t trimmed_len = end - input + 1;

  // Allocate memory for the new string
  char *result = malloc(trimmed_len + 1); // +1 for null terminator
  if (result == NULL) {
    return NULL;
  }

  // Copy the trimmed part and ensure null termination
  memcpy(result, input, trimmed_len);
  result[trimmed_len] = '\0';

  return result;
}
