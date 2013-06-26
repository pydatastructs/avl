#include <stdio.h>
#include <stdlib.h>

#include "avl.h"

int
compare_ints (void * compare_arg, void * a, void * b)
{
  int la, lb;
  la = (int) a;
  lb = (int) b;
  
  if (la < lb) {
    return -1;
  } else if (la > lb) {
    return +1;
  } else {
    return 0;
  }
}

int
int_printer (char * buffer, void * key)
{
  return sprintf (buffer, "%d", (int) key);
}

int
null_key_free (void * key) {
  return 0;
}

int
main (int argc, char ** argv)
{
  avl_tree * tree;
  unsigned int index;

  tree = avl_new_avl_tree (compare_ints, NULL);

  avl_insert_by_key (tree, (void *) 50, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 45, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 15, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 10, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 75, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 55, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 70, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 80, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 60, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 32, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 20, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 40, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 25, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 22, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 31, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  avl_insert_by_key (tree, (void *) 30, &index); avl_print_tree (tree, int_printer); avl_verify (tree);
  while (tree->length) {
    int num = 0;
    int any = 0;
    any = fscanf (stdin, "%d", &num);
    if (any < 1) {
      return 0;
    } else {
      fprintf (stdout, "deleting %d\n", num);
      avl_remove_by_key (tree, (void *) num, null_key_free);
      avl_print_tree (tree, int_printer);
      avl_verify (tree);
    }
  }
  return 0;
}
