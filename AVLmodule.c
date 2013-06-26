/* -*- Mode: C; indent-tabs-mode: nil -*- */

/*
 * Copyright (C) 1995-1997 by Sam Rushing <rushing@nightmare.com>
 * Copyright (C) 2005 by Germanischer Lloyd AG
 * Copyright (C) 2001-2005 by IronPort Systems, Inc.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of Sam
 * Rushing not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 *
 * SAM RUSHING DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL SAM RUSHING BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $Id: AVLmodule.c,v 2.14 2005/09/20 19:31:24 rushing Exp $ */

#include <Python.h>
#include "avl.h"

#ifdef  __cplusplus
extern "C" {
#endif

static PyObject *ErrorObject;

#ifndef PyDoc_STR
#define PyDoc_VAR(name)         static char name[]
#define PyDoc_STR(str)          (str)
#define PyDoc_STRVAR(name, str) PyDoc_VAR(name) = PyDoc_STR(str)
#endif

/* ----------------------------------------------------- */

/* Declarations for objects of type AVL tree */

/* We use an 'index cache'.  Whenever a lookup is done by index, we
 * cache the resultant node, so that if the next lookup is simply
 * <i+1>, we just get_successor() to find it.  Of course, invalidate
 * this node cache whenever there's been an insertion or deletion.
 *
 * This seems to speed up iterating over a large tree in an indirect
 * manner: If I create a tree of 100,000 nodes, the first few
 * iterations seem to take longer.  I think this is because the entire
 * tree needs to be paged into memory in order to access each element
 * individually.  The node cache is friendlier, in that only a small
 * number of nodes need be found in order to get the inorder
 * successor.
 */

typedef struct {
        PyObject_HEAD
        avl_tree        * tree;
        avl_node        * node_cache;
        int             cache_index;
        PyObject *      compare_function;
} avl_treeobject;

staticforward PyTypeObject Avl_treetype;

#define is_avl_tree_object(v)   ((v)->ob_type == &Avl_treetype)

/* forward declarations */
static avl_treeobject * newavl_treeobject(PyObject * compare_function);
static PyObject * avl_tree_slice (avl_treeobject *self, int ilow, int ihigh);
static avl_treeobject * avl_copy_avl_tree (avl_treeobject * source);

/* ---------------------------------------------------------------- */

static
int
avl_key_compare_for_python (void * compare_arg, void * a, void * b)
{
  avl_treeobject * self = (avl_treeobject *) compare_arg;

  if (!self->compare_function) {
    return PyObject_Compare ((PyObject *) a, (PyObject *) b);
  } else {
    PyObject * arglist, * py_result;

    arglist = Py_BuildValue ("(OO)", (PyObject *) a, (PyObject *) b);
    if (!arglist) {
      return 0;
    }
    py_result = PyEval_CallObject (self->compare_function, arglist);
    Py_DECREF (arglist);
    if (!py_result) {
      return 0;
    }
    if (!PyInt_Check (py_result)) {
      PyErr_SetString (PyExc_TypeError, "comparison function should return an int");
      return 0;
    } else {
      return PyInt_AsLong (py_result);
    }
  }
}

PyDoc_STRVAR (
  avl_tree_insert__doc__,
  "Insert an item into the tree"
  );

static PyObject *
avl_tree_insert(avl_treeobject * self, PyObject * args)
{
  PyObject * val;

  if (!PyArg_ParseTuple(args, "O", &val)) {
    return NULL;
  } else {
    unsigned int index = 0;
    Py_INCREF (val);
    if (avl_insert_by_key (self->tree, (void *) val, &index) != 0) {
      Py_DECREF (val);
      PyErr_SetString (ErrorObject, "error while inserting item");
      return NULL;
    } else {
      self->node_cache = NULL;
      return PyInt_FromLong (index);
    }
  }
}

PyDoc_STRVAR (
  avl_tree_remove__doc__,
  "Remove an item from the tree"
  );

/* remove_by_key's free_key_fun callback */

static
int
avl_tree_key_free_fun (void * key)
{
  Py_DECREF ((PyObject *) key);
  return 0;
}

static PyObject *
avl_tree_remove (avl_treeobject *self, PyObject *args)
{
  PyObject * val;

  if (!PyArg_ParseTuple(args, "O", &val)) {
    return NULL;
  } else {
    if (avl_remove_by_key (self->tree, (void *) val, avl_tree_key_free_fun) != 0) {
      PyErr_SetString (ErrorObject, "error while removing item");
      return NULL;
    } else {
      self->node_cache = NULL;
      Py_INCREF(Py_None);
      return Py_None;
    }
  }
}

PyDoc_STRVAR (
  avl_tree_lookup__doc__,
  "Return the first object comparing equal to the <key> argument"
  );

static PyObject *
avl_tree_lookup (avl_treeobject * self, PyObject * args)
{
  PyObject * key_val;
  PyObject * return_value;
  int result;

  if (!PyArg_ParseTuple (args, "O", &key_val)) {
    return NULL;
  } else {
    if (self->tree->length) {
      result = avl_get_item_by_key (self->tree, (void *) key_val, (void **) &return_value);
      if (result == 0) {
	/* success */
	Py_INCREF (return_value);
	return (return_value);
      } else {
	PyErr_SetObject (PyExc_KeyError, key_val);
	return NULL;
      }
    } else {
      PyErr_SetObject (PyExc_KeyError, key_val);
      return NULL;
    }
  }
}

PyDoc_STRVAR (
  avl_tree_span__doc__,
  "t.span (key) => (low, high)\n"
  "Returns a pair of indices (low, high) that span the range of <key>"
  );

static PyObject *
avl_tree_span (avl_treeobject * self, PyObject * args)
{
  PyObject * low_key, * high_key = NULL;
  unsigned int low, high;
  int result;

  if (!PyArg_ParseTuple (args, "O|O", &low_key, &high_key)) {
    return NULL;
  } else {
    if (self->tree->length) {
      /* only one key was specified */
      if (!high_key) {
	result = avl_get_span_by_key (self->tree,
                                      (void *) low_key,
                                      &low,
                                      &high);
	if (result == 0) {
	  /* success */
	  return Py_BuildValue ("(ii)", (int) low, (int) high);
	} else {
	  PyErr_SetString (ErrorObject, "error while locating key span");
	  return NULL;
	}
      } else {
	/* they specified two keys */
	result = avl_get_span_by_two_keys (self->tree,
                                           (void *) low_key,
                                           (void *) high_key,
                                           &low,
                                           &high);
	if (result == 0) {
	  /* success */
	  return Py_BuildValue ("(ii)", (int) low, (int) high);
	} else {
	  PyErr_SetString (ErrorObject, "error while locating key span");
	  return NULL;
	}
      }
    } else {
      return Py_BuildValue ("(ii)", 0, 0);
    }
  }
}

PyDoc_STRVAR (
  avl_tree_at_least__doc__,
  "Return the first object comparing greater to or equal to the <key> argument"
  );

static PyObject *
avl_tree_at_least (avl_treeobject * self, PyObject * args)
{
  PyObject * key_val;
  PyObject * return_value;
  int result;

  if (!PyArg_ParseTuple (args, "O", &key_val)) {
    return NULL;
  } else {
    if (self->tree->length) {
      result = avl_get_item_by_key_least (self->tree,
                                          (void *) key_val,
                                          (void **) &return_value);
      if (result == 0) {
	/* success */
	Py_INCREF (return_value);
	return (return_value);
      } else {
	PyErr_SetObject (PyExc_KeyError, key_val);
	return NULL;
      }
    } else {
      PyErr_SetObject (PyExc_KeyError, key_val);
      return NULL;
    }
  }
}

PyDoc_STRVAR (
  avl_tree_at_most__doc__,
  "Return the first object comparing less than or equal to the <key> argument"
  );
  

static PyObject *
avl_tree_at_most (avl_treeobject * self, PyObject * args)
{
  PyObject * key_val;
  PyObject * return_value;
  int result;

  if (!PyArg_ParseTuple (args, "O", &key_val)) {
    return NULL;
  } else {
    if (self->tree->length) {
      result = avl_get_item_by_key_most (self->tree,
                                         (void *) key_val,
                                         (void **) &return_value);
      if (result == 0) {
	/* success */
	Py_INCREF (return_value);
	return (return_value);
      } else {
	PyErr_SetObject (PyExc_KeyError, key_val);
	return NULL;
      }
    } else {
      PyErr_SetObject (PyExc_KeyError, key_val);
      return NULL;
    }
  }
}

PyDoc_STRVAR (
  avl_tree_has_key__doc__,
  "Does the tree contain an item comparing equal to <key>?"
  );

static PyObject *
avl_tree_has_key (avl_treeobject * self, PyObject * args)
{
  PyObject * key_val;
  PyObject * return_value;
  int result;

  if (!PyArg_ParseTuple (args, "O", &key_val)) {
    return NULL;
  } else {
    if (self->tree->length) {
      result = avl_get_item_by_key (self->tree,
                                    (void *) key_val,
                                    (void **) &return_value);
      if (result == 0) {
	/* success */
	return (Py_BuildValue ("i", 1));
      } else {
	return Py_BuildValue ("i", 0);
      }
    } else {
      return (Py_BuildValue ("i", 0));
    }
  }
}

#ifdef DEBUG_AVL
PyDoc_STRVAR (
  avl_tree_verify__doc__,
  "Verify the internal structure of the AVL tree (testing only)"
  );

static PyObject *
avl_tree_verify (avl_treeobject * self, PyObject * args)
{
  return (Py_BuildValue ("i", verify (self->tree)));
}

PyDoc_STRVAR (
  avl_tree_print_internal_structure__doc__,
  "Print the internal structure of the AVL tree (testing only)"
  );

static
int
avl_tree_key_printer (char * buffer, void * key)
{
  PyObject * repr_string;
  int length;

  repr_string = (PyObject *) PyObject_Repr ((PyObject *) key);
  if (repr_string) {
    strcpy (buffer, PyString_AsString (repr_string));
    length = PyString_Size (repr_string);
    Py_DECREF (repr_string);
    return length;
  } else {
    strcpy (buffer, "<couldn't print node>");
    return 21;
  }
}

static PyObject *
avl_tree_print_internal_structure (avl_treeobject * self, PyObject * args)
{
  print_tree (self->tree, avl_tree_key_printer);
  Py_INCREF (Py_None);
  return Py_None;
}

#endif


/*
 * This is an inorder tree-building: with careful synchronization
 * of the divide-and-conquer recursion and the source tree iteration
 * we can ask for the next key at the exact moment when we need it.
 * neat, huh!?
 * since we know that get_successor() is O(2) (see comments in avl_tree.py)
 * this algorithm is O(slice_size).
 */

/*
 * Todo: pull the (low == high) test up one level, this will
 * remove half the function calls.
 */

static
int
tree_from_tree (avl_node ** node,
                avl_node * parent,
                avl_node ** address,
                unsigned int low,
                unsigned int high)
{
  unsigned int midway = ((high - low) / 2) + low;
  if (low == high) {
    *address = NULL;
    return 1;
  } else {
    PyObject * item;
    avl_node * new_node;
    int left_height, right_height;

    new_node = avl_new_avl_node ((void *) 0, parent);
    if (!new_node) {
      return -1;
    }
    *address = new_node;
    AVL_SET_RANK (new_node, (midway-low)+1);
    left_height = tree_from_tree (node, new_node, &(new_node->left), low, midway);
    if (left_height < 0) {
      return -1;
    }

    item = (PyObject *) (*node)->key;
    Py_INCREF (item);
    new_node->key = (void *) item;
    *node = avl_get_successor (*node);

    right_height = tree_from_tree (node, new_node, &(new_node->right), midway+1, high);
    if (right_height  < 0) {
      return -1;
    }
    if (left_height > right_height) {
      AVL_SET_BALANCE (new_node, -1);
      return left_height + 1;
    } else if (right_height > left_height) {
      AVL_SET_BALANCE (new_node, +1);
      return right_height + 1;
    } else {
      AVL_SET_BALANCE (new_node, 0);
      return left_height + 1;
    }
  }
}

static char avl_tree_from_tree__doc__[] = "return a slice of the tree as a new tree";

static PyObject *
avl_tree_from_tree (avl_treeobject * self,
                    PyObject * args)
{
  int low=0;
  int high=self->tree->length;

  if (!PyArg_ParseTuple (args, "|ii", &low, &high)) {
    return NULL;
  }

  /* match Python slicing ops: */
  if (low < 0) low += self->tree->length;
  if (high < 0) high += self->tree->length;

  return avl_tree_slice (self, low, high);
}

static
int
slice_callback (unsigned int index, void * key, void * arg)
{
  /* arg is a list template */
  Py_INCREF ((PyObject *) key);
  return (PyList_SetItem ((PyObject *)arg, (int) index, (PyObject *) key));
}

static char avl_tree_slice_as_list__doc__[] = "return a slice of the tree as a list";

static PyObject *
avl_tree_slice_as_list (avl_treeobject * self, PyObject * args)
{
  PyObject * list;
  int ilow = 0;
  int ihigh = (int)self->tree->length;
  int result;

  if (!PyArg_ParseTuple (args, "|ii", &ilow, &ihigh)) {
    return NULL;
  }

  /* match Python slicing ops: */
  if (ilow < 0) ilow += self->tree->length;
  if (ihigh < 0) ihigh += self->tree->length;


  /* We are attempting to match Python slicing on list objects,
   * which is incredibly lenient. Basically, it is impossible to
   * get an exception.
   */

  if (ilow < 0) ilow = 0;
  if (ihigh < 0) ihigh = 0;
  if ((unsigned int)ihigh > self->tree->length) {
    ihigh = (int)self->tree->length;
  }
  if (ilow > ihigh) {
    ilow = ihigh;
  }

  if (!(list = PyList_New (ihigh - ilow))) {
    return NULL;
  }
  if (ihigh - ilow) {
    result = avl_iterate_index_range (self->tree,
                                      slice_callback,
                                      (unsigned int) ilow,
                                      (unsigned int) ihigh,
                                      (void *) list);
    if (result != 0) {
      PyErr_SetString (ErrorObject, "error while accessing slice");
      return NULL;
    }
  }
  return list;
}

static struct PyMethodDef avl_tree_methods[] = {
  {"insert",    (PyCFunction)avl_tree_insert,           1,      avl_tree_insert__doc__},
  {"remove",    (PyCFunction)avl_tree_remove,   1,      avl_tree_remove__doc__},
  {"lookup",    (PyCFunction)avl_tree_lookup,   1,      avl_tree_lookup__doc__},
  {"has_key",   (PyCFunction)avl_tree_has_key,  1,      avl_tree_has_key__doc__},
  {"slice_as_list",     (PyCFunction)avl_tree_slice_as_list,1,  avl_tree_slice_as_list__doc__},
  {"span",      (PyCFunction)avl_tree_span,     1,      avl_tree_span__doc__},
  {"at_least",  (PyCFunction)avl_tree_at_least, 1,      avl_tree_at_least__doc__},
  {"at_most",   (PyCFunction)avl_tree_at_most,  1,      avl_tree_at_most__doc__},
  {"slice_as_tree",     (PyCFunction)avl_tree_from_tree,1,      avl_tree_from_tree__doc__},
#ifdef DEBUG_AVL
  {"verify",    (PyCFunction)avl_tree_verify,   1,      avl_tree_verify__doc__},
  {"print_internal_structure",  (PyCFunction)avl_tree_print_internal_structure, 1,      avl_tree_print_internal_structure__doc__},
#endif
  {NULL,                NULL}           /* sentinel */
};

/* ---------- */


static avl_treeobject *
newavl_treeobject (PyObject * compare_function)
{
  avl_treeobject *self;

  self = PyObject_New (avl_treeobject, &Avl_treetype);
  if (self == NULL) {
    return NULL;
  }

  self->tree = avl_new_avl_tree (avl_key_compare_for_python, (void *) self);
  if (!self->tree) {
    PyObject_Del (self);
    return NULL;
  }
  self->node_cache = NULL;
  self->cache_index = 0;
  self->compare_function = compare_function;
  Py_XINCREF (compare_function);
  return self;
}

static void
avl_tree_dealloc(avl_treeobject *self)
{
  avl_free_avl_tree (self->tree, avl_tree_key_free_fun);
  PyObject_Del(self);
}

static
int
avl_tree_print_helper (avl_node * node,
                       int * index,
                       FILE * fp)
{
  if (node->left) {
    if (avl_tree_print_helper (node->left, index, fp) != 0) {
      return -1;
    }
  }
  if ((*index)) {
    fprintf (fp, ", ");
  }
  if ((PyObject_Print ((PyObject*) node->key, fp, 0)) != 0) {
    return -1;
  } else {
    (*index)++;
  }
  if (node->right) {
    if (avl_tree_print_helper (node->right, index, fp) != 0) {
      return -1;
    }
  }
  return 0;
}

static
int
avl_tree_print(avl_treeobject *self, FILE *fp, int flags)
{
  if (!self->tree->length) {
    fprintf (fp, "[]");
  } else {
    int index = 0;

    fprintf (fp, "[");
    if ((avl_tree_print_helper (self->tree->root->right, & index, fp)) != 0) {
      return -1;
    }
    fprintf (fp, "]");
  }
  return 0;
}


static
PyObject *
avl_tree_getattr(avl_treeobject *self, char *name)
{
        /* XXXX Add your own getattr code here */
        return Py_FindMethod(avl_tree_methods, (PyObject *)self, name);
}

static
int
avl_tree_setattr(avl_treeobject *self, char *name, PyObject *v)
{
        /* XXXX Add your own setattr code here */
        return -1;
}

static
PyObject *
avl_tree_repr (avl_treeobject *self)
{
  PyObject * s;
  PyObject * comma;
  avl_node * node;
  unsigned int i;

  if (self->tree->length) {
    s = PyString_FromString ("[");
    comma = PyString_FromString (", ");
    /* find leftmost node */
    node = self->tree->root->right;
    while (node->left) {
      node = node->left;
    }
    for (i=0; i < self->tree->length; i++) {
      if (i > 0) {
        PyString_Concat (&s, comma);
      }
      PyString_ConcatAndDel (&s, PyObject_Repr ((PyObject *)node->key));
      node = avl_get_successor (node);
    }
    Py_XDECREF (comma);
    PyString_ConcatAndDel (&s, PyString_FromString ("]"));
    return s;
  } else {
    return PyString_FromString ("[]");
  }
}

/* Code to handle accessing AVL tree objects as sequence objects */

static
int
avl_tree_length (avl_treeobject *self)
{
  return (int) self->tree->length;
}

/* todo: support tree+list */

static
PyObject *
avl_tree_concat (avl_treeobject *self, avl_treeobject *bb)
{
  avl_treeobject * self_copy;
  unsigned int bb_node_counter = bb->tree->length;

  self_copy = avl_copy_avl_tree ((avl_treeobject *) self);
  if (!self_copy) {
    return NULL;
  }

  if (bb_node_counter) {
    avl_node * bb_node;

    /* find the leftmost node of bb */
    bb_node = bb->tree->root->right;
    while (bb_node->left) {
      bb_node = bb_node->left;
    }

    /*
     * iterate over the items in bb, inserting
     * them into self_copy
     */
    while (bb_node_counter--) {
      unsigned int ignore;
      Py_INCREF ((PyObject *)bb_node->key);
      if (avl_insert_by_key (self_copy->tree, (void *) bb_node->key, &ignore) != 0) {
        avl_tree_dealloc (self_copy);
        return NULL;
      }
      bb_node = avl_get_successor (bb_node);
    }
  }
  return (PyObject *) self_copy;
}

static
PyObject *
avl_tree_repeat(avl_treeobject * self, int n)
{
  PyErr_SetString (PyExc_TypeError,"repeat operation undefined");
  return NULL;
}

static
PyObject *
avl_tree_item (avl_treeobject *self, int i)
{
  void * value;
  unsigned int index = (unsigned int) i;
  
  /* Python takes care of negative indices for us, so if
   * i is negative, that is an error. */

  /* range-check the index */
  if (index >= self->tree->length || i < 0) {
    PyErr_SetString (PyExc_IndexError, "tree index out of range");
    return NULL;
  } else {
    /* get the python object, and store in <value> */

    /* index cache */
    if (self->node_cache && (index == (self->cache_index + 1))) {
      self->node_cache = avl_get_successor (self->node_cache);
      self->cache_index++;
      value = self->node_cache->key;
    }
    if (avl_get_item_by_index (self->tree, index, &value) != 0) {
      PyErr_SetString (ErrorObject, "error while accessing item");
      return NULL;
    } else {
      Py_INCREF ((PyObject *)value);
      return (PyObject *)value;
    }
  }
}

static
PyObject *
avl_tree_slice (avl_treeobject *self, int ilow, int ihigh)
{
  avl_treeobject * new_tree;
  unsigned int m;
  avl_node * node;

  /* We are attempting to match Python slicing on list objects,
   * which is incredibly lenient. Basically, it is impossible to
   * get an exception.
   */

  /* By the time we are called, the Python internals have
   * already added the length of self to ilow and ihigh if they are
   * negative. However, the values can still be negative or too large.
   */
  if (ilow < 0) ilow = 0;
  if (ihigh < 0) ihigh = 0;
  if ((unsigned int)ihigh > self->tree->length) {
    ihigh = (int)self->tree->length;
  }

  new_tree = newavl_treeobject (self->compare_function);
  if (!new_tree) {
    return NULL;
  }

  /* return empty tree in this degenerate case: */
  if (ihigh <= ilow) {
    return (PyObject *)new_tree;
 }

  /* locate node <ilow> */
  node = self->tree->root->right;
  m = (unsigned int) ilow + 1;
  while (1) {
    if (m < AVL_GET_RANK(node)) {
      node = node->left;
    } else if (m > AVL_GET_RANK(node)) {
      m = m - AVL_GET_RANK(node);
      node = node->right;
    } else {
      break;
    }
  }

  if (tree_from_tree (&node,
                      new_tree->tree->root,
                      &(new_tree->tree->root->right),
                      0,
                      ihigh - ilow) < 0) {
    PyErr_SetString (ErrorObject, "something went amiss whilst building the tree!");
    return NULL;
  }
  new_tree->tree->length = ihigh - ilow;
  return (PyObject *) new_tree;
}

static PySequenceMethods avl_tree_as_sequence = {
        (inquiry)avl_tree_length,               /*sq_length*/
        (binaryfunc)avl_tree_concat,            /*sq_concat*/
        (intargfunc)avl_tree_repeat,            /*sq_repeat*/
        (intargfunc)avl_tree_item,              /*sq_item*/
        (intintargfunc)avl_tree_slice,          /*sq_slice*/
        (intobjargproc)0,                       /*sq_ass_item*/
        (intintobjargproc)0,                    /*sq_ass_slice*/
};

/* -------------------------------------------------------------- */

PyDoc_STRVAR (
  Avl_treetype__doc__,
  "A dual-personality object, can act like a sequence and a dictionary.  Implemented with an AVL tree"
  );

static PyTypeObject Avl_treetype = {
        PyObject_HEAD_INIT (NULL)
        0,                              /*ob_size*/
        "avl.tree",                     /*tp_name*/
        sizeof(avl_treeobject),         /*tp_basicsize*/
        0,                              /*tp_itemsize*/
        /* methods */
        (destructor)avl_tree_dealloc,   /*tp_dealloc*/
        (printfunc)avl_tree_print,      /*tp_print*/
        (getattrfunc)avl_tree_getattr,  /*tp_getattr*/
        (setattrfunc)avl_tree_setattr,  /*tp_setattr*/
        (cmpfunc)0,                     /*tp_compare*/
        (reprfunc)avl_tree_repr,        /*tp_repr*/
        0,                              /*tp_as_number*/
        &avl_tree_as_sequence,          /*tp_as_sequence*/
        0,                      /*tp_as_mapping*/
        (hashfunc)0,            /*tp_hash*/
        (ternaryfunc)0,         /*tp_call*/
        (reprfunc)0,            /*tp_str*/

        0,                         /*tp_getattro*/
        0,                         /*tp_setattro*/
        0,                         /*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT,        /*tp_flags*/
        Avl_treetype__doc__ /* Documentation string */
};

/* End of code for AVL tree objects */
/* -------------------------------------------------------- */

static
int
avl_copy_avl_node (avl_node * source_node,
                   avl_node * dest_parent,
                   avl_node ** dest_node)
{
  avl_node * new_node = avl_new_avl_node(source_node->key, dest_parent);

  if (!new_node) {
    return -1;
  } else {
    Py_INCREF ((PyObject *) new_node->key);
    new_node->rank_and_balance = source_node->rank_and_balance;
    if (source_node->left) {
      if (avl_copy_avl_node (source_node->left,
                             new_node,
                             &(new_node->left)) != 0) {
        return -1;
      }
    } else {
      new_node->left = NULL;
    }
    if (source_node->right) {
      if (avl_copy_avl_node (source_node->right,
                             new_node,
                             &(new_node->right)) != 0) {
        return -1;
      }
    } else {
      new_node->right = NULL;
    }
    *dest_node = new_node;
    return 0;
  }
}

static
avl_treeobject *
avl_copy_avl_tree (avl_treeobject * source)
{
  avl_treeobject * dest;

  if (!(dest = newavl_treeobject (source->compare_function))) {
    return NULL;
  } else if (source->tree->length) {
    if (avl_copy_avl_node (source->tree->root->right,
                           dest->tree->root,
                           &(dest->tree->root->right)) != 0) {
      avl_tree_dealloc(dest);
      return NULL;
    } else {
      dest->tree->length = source->tree->length;
      return dest;
    }
  } else {
    return dest;
  }
}

/*
 * not so cleanly separated from the avl 'library',
 * since it builds the tree directly.
 */

/*
 * Todo: pull the (low == high) test up one level, this will
 * remove half the function calls.
 */

static
int
tree_from_list (PyObject * list,
                avl_node * parent,
                avl_node ** address,
                unsigned int low,
                unsigned int high)
{
  unsigned int midway = ((high - low) / 2) + low;
  if (low == high) {
    *address = NULL;
    return 1;
  } else {
    PyObject * item = PyList_GetItem (list, midway);
    avl_node * new_node;
    int left_height, right_height;

    new_node = avl_new_avl_node ((void *) item, parent);
    if (!new_node) {
      return -1;
    }
    *address = new_node;
    Py_INCREF (item);
    AVL_SET_RANK (new_node, (midway-low)+1);
    left_height = tree_from_list (list, new_node, &(new_node->left), low, midway);
    if (left_height < 0) {
      return -1;
    }
    right_height = tree_from_list (list, new_node, &(new_node->right), midway+1, high);
    if (right_height  < 0) {
      return -1;
    }
    if (left_height > right_height) {
      AVL_SET_BALANCE (new_node, -1);
      return left_height + 1;
    } else if (right_height > left_height) {
      AVL_SET_BALANCE (new_node, +1);
      return right_height + 1;
    } else {
      AVL_SET_BALANCE (new_node, 0);
      return left_height + 1;
    }
  }
}

/* construct a new avl tree from a list (sorts the list as a side-effect!) */

static
PyObject *
avl_new_avl_from_list (PyObject * self, /* not used */
                       PyObject * args)
{
  PyObject * list;
  PyObject * compare_function = NULL;
  avl_treeobject * tree;
  unsigned int length;

  if (!PyArg_ParseTuple (args, "O!|O", &PyList_Type, &list, &compare_function)) {
    return NULL;
  }
  /* sort the list */
  if (PyList_Sort (list) != 0) {
    return NULL;
  }

  tree = newavl_treeobject (compare_function);
  if (!tree) {
    return NULL;
  }
  length = PyList_Size(list);
  if (tree_from_list (list,
                      tree->tree->root,
                      &(tree->tree->root->right),
                      0,
                      length) < 0) {
    PyErr_SetString (ErrorObject, "something went amiss whilst building the tree!");
    return NULL;
  }
  tree->tree->length = length;
  return (PyObject *) tree;
}

PyDoc_STRVAR (
  avl_newavl__doc__,
  "With no arguments, returns a new and empty tree.\n"
  "Given a list, it will return a new tree containing the elements\n"
  "  of the list, and will sort the list as a side-effect\n"
  "Given a tree, will return a copy of the original tree\n"
  "An optional second argument is a key-comparison function\n"
  );
  
static
PyObject *
avl_newavl(PyObject * self,     /* Not used */
           PyObject * args)
{
  PyObject * arg = NULL;
  PyObject * compare_function = NULL;

  if (!PyArg_ParseTuple(args, "|OO", &arg, &compare_function)) {
    return NULL;
  }
  if (!arg || arg == Py_None) {
    return (PyObject *) newavl_treeobject (compare_function);
  } else if (PyList_Check(arg)) {
    return avl_new_avl_from_list ((PyObject *) NULL, args);
  } else if (is_avl_tree_object (arg)) {
    return (PyObject *) avl_copy_avl_tree ((avl_treeobject *) arg);
  } else {
    PyErr_SetString (ErrorObject, "newavl() expects a list, a tree, or no arguments");
    return NULL;
  }
}

/* List of methods defined in the module */

static struct PyMethodDef avl_methods[] = {
        {"newavl",      avl_newavl,     1,      avl_newavl__doc__},
        {NULL,          NULL}           /* sentinel */
};

/* Initialization function for the module (*must* be called initavl) */

PyDoc_STRVAR (
  avl_module_documentation,
  "Implements a dual-personality object (that can act like a sequence _and_ a dictionary) with AVL trees."
  );

void
initavl(void)
{
  PyObject *m, *d, *v;

  /* Create the module and add the functions */
  m = Py_InitModule4 (
    "avl",
    avl_methods,
    avl_module_documentation,
    (PyObject*)NULL,
    PYTHON_API_VERSION
    );

  /* Add some symbolic constants to the module */
  d = PyModule_GetDict(m);
  ErrorObject = PyString_FromString("avl.error");
  PyDict_SetItemString(d, "error", ErrorObject);

  v = PyString_FromString ("2.1.3");
  PyDict_SetItemString(d, "__version__", v);
  Py_XDECREF(v);

  if (PyType_Ready(&Avl_treetype) < 0) {
    return;
  }
  
  /* Check for errors */
  if (PyErr_Occurred()) {
    Py_FatalError("can't initialize module avl");
  }
}

#ifdef  __cplusplus
}
#endif
