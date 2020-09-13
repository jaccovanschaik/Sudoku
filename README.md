# Sudoku
A sudoku solver.

This program solves sudokus in two steps. The first is to find empty
cells where only a single value (given the other values in the row,
column and 3x3 square that the cell is in) is available, filling the
cell with that value and then doing the same again with the new sudoku.

The second step, which is only executed if the first one couldn't solve
the sudoku, will find all possible values for the first empty cell, fill
that cell with all of them in turn, and then recursively try to solve
the new sudoku. If at any point an empty cell is found where no value at
all is available, this means that this particular sudoku is unsolvable,
and the recursion goes back up a level to try the previous cell with the
next available value. this all continues until either all the empty
cells have been filled, or no solution could be found.

The available values for each row, column and 3x3 square are kept as
bitmasks (if bit 0 is set then 1 is still available, if bit 1 is set
then 2 is still available etc.) This allows us to use bitwise ANDs over
the available values for a row, column and square to quickly find the
available values for a given cell.

A number of sudokus are provided, some solvable using only step 1, some
also requiring step 2. In addition, a template is provided allowing you
to try some sudokus of your own.

The Makefile contains a `DEBUG` setting. Set this to 1 to see progress
as the program runs. Note that this artificially slows down the program
*considerably* so that you actually can see something. Otherwise it
would all be over in a flash.

Have fun!
