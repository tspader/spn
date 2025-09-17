#include "md4c.h"
#include "stdio.h"
#include "string.h"

const char* markdown = "# Band Members" "\n"
  "## Jerry" "\n"
  "## Bobby" "\n"
  "## Phil" "\n"
  "```cpp" "\n"
  "// a comment!" "\n"
  "```" "\n"
  "Man, I regret making such a long list." "\n";

const char* md_block_type_to_str(MD_BLOCKTYPE kind) {
  switch (kind) {
    case MD_BLOCK_DOC: return "MD_BLOCK_DOC";
    case MD_BLOCK_QUOTE: return "MD_BLOCK_QUOTE";
    case MD_BLOCK_OL: return "MD_BLOCK_OL";
    case MD_BLOCK_UL: return "MD_BLOCK_UL";
    case MD_BLOCK_LI: return "MD_BLOCK_LI";
    case MD_BLOCK_HR: return "MD_BLOCK_HR";
    case MD_BLOCK_H: return "MD_BLOCK_H";
    case MD_BLOCK_CODE: return "MD_BLOCK_CODE";
    case MD_BLOCK_HTML: return "MD_BLOCK_HTML";
    case MD_BLOCK_P: return "MD_BLOCK_P";
    case MD_BLOCK_TABLE: return "MD_BLOCK_TABLE";
    case MD_BLOCK_THEAD: return "MD_BLOCK_THEAD";
    case MD_BLOCK_TBODY: return "MD_BLOCK_TBODY";
    case MD_BLOCK_TR: return "MD_BLOCK_TR";
    case MD_BLOCK_TH: return "MD_BLOCK_TH";
    case MD_BLOCK_TD: return "MD_BLOCK_TD";
  }
  return "UNKNOWN";
}

int on_text(MD_TEXTTYPE kind, const char* text, MD_SIZE size, void* user_data) {
  printf("%.*s\n", size, text);
  return 0;
}

int on_span(MD_SPANTYPE kind, void* detail, void* user_data) {
  return 0;
}

int on_exit_block(MD_BLOCKTYPE kind, void* detail, void* user_data) {
  return 0;
}

int on_enter_block(MD_BLOCKTYPE kind, void* detail, void* user_data) {
  printf("on_enter_block: %s\n", md_block_type_to_str(kind));
  switch (kind) {
    case MD_BLOCK_H: {
      MD_BLOCK_H_DETAIL* heading = (MD_BLOCK_H_DETAIL*)detail;
      printf("heading: %d\n", heading->level);
      break;
    }
    case MD_BLOCK_CODE: {
      MD_BLOCK_CODE_DETAIL* code = (MD_BLOCK_CODE_DETAIL*)detail;
      printf("found a code block (%s)\n", code->lang.text);
      break;
    }
    default: {
      break;
    }
  }
  return 0;
}

int main(int num_args, const char** args) {
  MD_PARSER md = {
    .enter_block = on_enter_block,
    .leave_block = on_exit_block,
    .enter_span = on_span,
    .leave_span = on_span,
    .text = on_text,
  };
  md_parse(markdown, strlen(markdown), &md, NULL);
  return 0;
}
