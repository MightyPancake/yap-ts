#include "tree_sitter/api.h"
#include "ts_yap.h"
#include "utils/utils.h"
#include "yap/types.h"

#define err_printf(...) fprintf(stderr, __VA_ARGS__)

//Max line_no = 9999 (4 places)
void yap_helper_print_lineno_normalized(int line_no){
  int w = 4;
  int lw = 1;
  if (line_no > 9) lw++;
  if (line_no > 99) lw++;
  if (line_no > 999) lw++;
  for (int i=lw; i<w; i++){
    err_printf(" ");
  }
  err_printf("%d", line_no);
}

// │
void yap_helper_print_line(int line_no, bool is_error){
  err_printf(aesc_style("2")"%s", is_error ? aesc_red : aesc_white);
  yap_helper_print_lineno_normalized(line_no+1);
  err_printf(aesc_reset " ");
}

void yap_print_error(yap_error err){
  yap_source* src = err.src;
  yap_code_pos start = err.range.start;
  yap_code_pos end = err.range.end;
  //TODO: Implement
  // char* src_stack = strus_newf("In file: %s\n"err.src->path);
  err_printf("File "aesc_cyan"%s"aesc_reset" at "aesc_yellow "%d:%d\n" aesc_reset, src->path, start.line+1, start.column);
  err_printf("└─ used in " aesc_cyan"%s"aesc_reset"\n", "some/file.yap");
  yap_source* parent = (yap_source*)src->parent;
  while(parent){
    err_printf("└─ used in " aesc_cyan"%s"aesc_reset"\n", parent->path);
    parent = (yap_source*)parent->parent;
  }
  err_printf(aesc_red aesc_style("1") "ERROR: " aesc_style("3") "%s\n\n" aesc_reset, err.msg);
  // err_printf("%s\n", src->content);
  uint start_byte = start.offset;
  int newlines_before = 2;
  int newlines_after = 3;
  int line = start.line;

  newlines_before++;
  while(start_byte){
    if (src->content[start_byte--] == '\n'){
      newlines_before--;
      if (!newlines_before){
        start_byte+=2;  //skip 2, because postfix --
        break;
      }else{
        line--;
      }
    }
  }
  // if (!newlines_before) start_byte++;
  // printf("offsets: %d-%d\nstart_offset: %d\n", start.offset, end.offset, start_byte);
  char* c = &(src->content[start_byte]);
  yap_helper_print_line(line, start.line<=line && line<=end.line);
  int offset = start_byte;
  bool style_flag = false;
  while(true){
    if (style_flag) err_printf(aesc_reset);
      if (start.offset <= offset && offset < end.offset){
        err_printf(aesc_red aesc_style("27") aesc_style("4") aesc_style("5"));
        style_flag = true;
      // }else if (start.line <= line && line <= end.line){
      //   err_printf(aesc_style("100") aesc_black);
      //   style_flag = true;
      }else if (style_flag){
        err_printf(aesc_reset);
        style_flag = false;
      }
    // printf("\noffset = %d\n", offset);
    if (*c == '\n'){
      err_printf(aesc_reset"\n");
      if (line++==end.line+newlines_after){
        break;
      }
      yap_helper_print_line(line, start.line<=line && line<=end.line);
    }else if(*c == '\t'){
      err_printf("   ");
    }else if (*c == '\0'){
      err_printf("\n");
      break;
    }else{
      err_printf("%c", *c);
    }
    c++;
    offset++;
  }
}

yap_error yap_node_error(yap_source* src, TSNode node, char* msg){
  return (yap_error){
    .kind=yap_error_pos,
    .msg=strus_copy(msg),
    .src=src,
    .range=yap_node_get_range(node)
  };
}

yap_code_pos yap_node_get_start_point(TSNode node){
  TSPoint p = ts_node_start_point(node);
  return (yap_code_pos){
    .line=p.row,
    .column=p.column,
    .offset=ts_node_start_byte(node)
  };
}

yap_code_pos yap_node_get_end_point(TSNode node){
  TSPoint p = ts_node_end_point(node);
  return (yap_code_pos){
    .line=p.row,
    .column=p.column,
    .offset=ts_node_end_byte(node)
  };
}

yap_code_range yap_node_get_range(TSNode node){
  return (yap_code_range){
    .start=yap_node_get_start_point(node),
    .end=yap_node_get_end_point(node)
  };
}

#undef err_printf
