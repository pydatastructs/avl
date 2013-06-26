# -*- Mode: Python -*-

# give the avl module a good workout.

import avl
import random
import time

class timer:
    def __init__ (self):
        self.start = time.time()
    def end (self):
        print '%f seconds' % (time.time() - self.start)

def generate_test_numbers (n=10000):
    return [random.randint(0, 1000000) for x in xrange (n)]

def fill_up (tree, nums):
    print 'filling up...'
    t = timer()
    for num in nums:
        tree.insert(num)
    t.end()

def random_indices_tree (length):
    t = avl.newavl()
    # build a 'list' of indices
    for num in range(length):
        t.insert(num)
    t2 = avl.newavl()
    for x in range(length):
        i = random.choice(t)
        t2.insert((x,i))
        t.remove(i)
    # return the tree as a list
    return map (lambda x: x[1], t2)

def random_indices_list (length):
    choices = range(length)
    result = range(length)
    for i in range(length):
        n = random.choice(choices)
        result[i] = n
        choices.remove(n)
    return result

# we're careful to avoid timing the random module.

def empty (tree):
    indices = random_indices_tree (len(tree))
    values = range(len(tree))
    for i in range(len(tree)):
        values[i] = tree[indices[i]]
    print 'emptying the tree...'
    t = timer()
    for value in values:
        tree.remove (value)
    t.end()

def random_slice (length):
    left = random.randint (0, length)
    right = random.randint (0, length)
    if (left > right):
        # tuples are just _too_ cool.
        left, right = right, left
    return (left, right)

def slice_test (tree, num_slices=100):
    print 'computing slices...'
    slices = map (random_slice, [len(tree)]*num_slices)
    print 'slicing %d times...' % num_slices
    t = timer()
    for left, right in slices:
        slice = tree[left:right]
    t.end()

def test(n):
    tree = avl.newavl()
    print 'generating random numbers...'
    t = timer(); nums = generate_test_numbers (n); t.end()
    fill_up(tree,nums)
    slice_test(tree)
    empty(tree)

import string
import sys

if __name__ == '__main__':
    print sys.argv
    if len(sys.argv) > 1:
        iterations = string.atoi (sys.argv[1])
        if len(sys.argv) > 2:
            test_size = string.atoi (sys.argv[2])
        else:
            test_size = 10000
    else:
        iterations = 100
        test_size = 10000
    for i in range(iterations):
        print 'test %d' % i
        test(test_size)
