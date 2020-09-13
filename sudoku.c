/*
 * Sudoku solver.
 *
 * Copyright:	(c) 2010 Jacco van Schaik (jacco@jaccovanschaik.net)
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

/*
 * For every row, column and 3x3 square we maintain a bit mask where the set
 * bits represent the numbers that are still available in that row, column or
 * square. Using bit masks allows us to do bitwise ANDs to find out which
 * numbers are still available for a particular cell.
 */

typedef struct {
    uint16_t row[9];        // Available numbers for each row.
    uint16_t col[9];        // Available numbers for each column.
    uint16_t sqr[3][3];     // Available numbers for each 3x3 square.
    uint8_t  cell[9][9];    // Contents of each cell (0 means empty).
} Sudoku;

// The bit mask that represents a value in a cell.
#define BIT_FOR_VALUE(val) (1 << ((val) - 1))

/*
 * Dump the contents of sudoku grid <sudoku> to <fp>.
 */
static void dump_sudoku(FILE *fp, const Sudoku *sudoku)
{
    int row, col;

    for (row = 0; row < 9; row++) {
        if ((row % 3) == 0) {
            fprintf(fp, "+-------+-------+-------+\n");
        }

        fprintf(fp, "|");

        for (col = 0; col < 9; col++) {
            if (sudoku->cell[row][col] == 0)
                fprintf(fp, "  ");
            else
                fprintf(fp, "%2hhu", sudoku->cell[row][col]);

            if ((col % 3) == 2)
                fprintf(fp, " |");
        }

        fprintf(fp, "\n");

        if (row == 8) {
            fprintf(fp, "+-------+-------+-------+\n");
        }
    }
}

/*
 * Find the next empty cell in <sudoku>. If found, returns 1 and fills <row> and
 * <col> with its location. Otherwise returns 0.
 */
static int find_empty_cell(const Sudoku *sudoku, int *row, int *col)
{
    for (*row = 0; *row < 9; (*row)++) {
        for (*col = 0; *col < 9; (*col)++) {
            if (sudoku->cell[*row][*col] == 0) return 1;
        }
    }

    return 0;
}

/*
 * Fill the cell at row <row> and column <col> in <sudoku> with <value> and
 * update the bitmasks that represent the now available values for the row,
 * column and square that this cell is in.
 */
static void fill_cell(Sudoku *sudoku, int row, int col, int value)
{
    // Fill the current cell...
    sudoku->cell[row][col] = value;

    // Update the available values for this row, column and square.
    sudoku->row[row]          &= ~(BIT_FOR_VALUE(value));
    sudoku->col[col]          &= ~(BIT_FOR_VALUE(value));
    sudoku->sqr[row/3][col/3] &= ~(BIT_FOR_VALUE(value));
}

/*
 * Return a bitmask containing the available values at row <row>, column <col>
 * in <sudoku>.
 */
static int available_values_in_cell(const Sudoku *sudoku, int row, int col)
{
    uint16_t available_in_row = sudoku->row[row];
    uint16_t available_in_col = sudoku->col[col];
    uint16_t available_in_sqr = sudoku->sqr[row/3][col/3];

    return available_in_row & available_in_col & available_in_sqr;
}

/*
 * Method 1. As long as there are empty cells where only one value is possible,
 * set that cell to that value. If this solves the entire sudoku return 1,
 * otherwise return 0. In either case, the sudoku is filled in as far as we
 * could manage.
 */
static int step1(Sudoku *sudoku)
{
    /*
     * An array to help us identify if a cell has only one available value. The
     * index is a value bitmask, the elements are the one available value if
     * only one is available, otherwise it is 0. The number of possible bit
     * masks (given we have 9 bits) is 2^9 (= 512).
     */
    static const int only_possible_value[1 << 9] = {
        [0x001] = 1,
        [0x002] = 2,
        [0x004] = 3,
        [0x008] = 4,
        [0x010] = 5,
        [0x020] = 6,
        [0x040] = 7,
        [0x080] = 8,
        [0x100] = 9
    };

    int row, col;
    int empty_cell_count;

next_single_value_cell:

#if DEBUG == 1
    fprintf(stderr, "\033[H");

    dump_sudoku(stderr, sudoku);

    usleep(100000);
#endif

    empty_cell_count = 0;

    // Let's go find an empty cell that has only one available value.

    for (row = 0; row < 9; row++) {
        for (col = 0; col < 9; col++) {
            if (sudoku->cell[row][col] != 0) continue;  // Cell not empty.

            empty_cell_count++;

            uint16_t available_values =                 // Available values.
                available_values_in_cell(sudoku, row, col);
            uint8_t the_only_possible_value =           // Is there only one?
                only_possible_value[available_values];

            if (the_only_possible_value > 0) {
                // There is! Put it in this cell, then go find the next cell.
                fill_cell(sudoku, row, col, the_only_possible_value);
                goto next_single_value_cell;
            }
        }
    }

    if (empty_cell_count == 0) {
        // No more empty cells: we're done!

        fprintf(stdout, "Solution:\n"); dump_sudoku(stdout, sudoku);

        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Method 2. Fill the first empty cell with all possible values in turn, and
 * call myself recursively each time to try and solve that new sudoku. Returns 1
 * if successful, 0 if not.
 */
static int step2(const Sudoku *sudoku)
{
    int value, row, col;

    if (!find_empty_cell(sudoku, &row, &col)) {
        // No empty cells left. We're done!

        fprintf(stdout, "Solution:\n"); dump_sudoku(stdout, sudoku);

        return 1;
    }

    // Find out which values are available for this cell.

    uint16_t available_values;

    if ((available_values = available_values_in_cell(sudoku, row, col)) == 0) {
        // Nothing available for this cell, i.e. sudoku is unsolvable.
        return 0;
    }

#if DEBUG == 1
    fprintf(stderr, "\033[H");

    dump_sudoku(stderr, sudoku);

    usleep(10000);
#endif

    /*
     * Now fill this cell with each of the available values in turn, and see if
     * we can recursively step2 the rest of the sudoku.
     */

    for (value = 1; value <= 9; value++) {
        // If <value> is available...
        if (available_values & (BIT_FOR_VALUE(value))) {
            Sudoku new_sudoku;

            // Copy the current sudoku...
            memcpy(&new_sudoku, sudoku, sizeof(Sudoku));

            // Set the cell to <value>.
            fill_cell(&new_sudoku, row, col, value);

            // And recursively solve this new sudoku.
            if (step2(&new_sudoku)) {
                // Solution found, somewhere down the line. We're done!

                return 1;
            }
        }
    }

    // No solution found.

    return 0;
}

/*
 * Give some usage info for <argv0> and exit with exit code <exitcode>.
 */
static void usage(const char *argv0, int exitcode)
{
    fprintf(stderr, "Usage: %s <sudoku_input_file>\n", basename(argv0));

    exit(exitcode);
}

/*
 * <sudoku> has just been read from disk. Initialize the bitmasks for the rows,
 * columns and 3x3 squares.
 */
static void init_sudoku(Sudoku *sudoku)
{
    int row, col;

    // Initially mark all rows, columns and squares as "all values available".

    for (row = 0; row < 9; row++) sudoku->row[row] = 0x1FF; // bits 0-8.
    for (col = 0; col < 9; col++) sudoku->col[col] = 0x1FF;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            sudoku->sqr[row][col] = 0x1FF;
        }
    }

    /*
     * Now go through all the filled cells and update the available values for
     * the row, column and square that they are in.
     */

    for (row = 0; row < 9; row++) {
        for (col = 0; col < 9; col++) {
            uint8_t value = sudoku->cell[row][col];

            if (value == 0) continue;

            uint8_t sq_row = row / 3;
            uint8_t sq_col = col / 3;

            sudoku->row[row]            &= ~(BIT_FOR_VALUE(value));
            sudoku->col[col]            &= ~(BIT_FOR_VALUE(value));
            sudoku->sqr[sq_row][sq_col] &= ~(BIT_FOR_VALUE(value));
        }
    }
}

/*
 * Check character <c> in <text>, We expect it to be <exp>, if it isn't complain
 * on <fp> (indicating the error is in file <file> on line <line>) and exit with
 * an error code.
 */
static void expect(const char *text, int c, char exp,
        FILE *fp, const char *file, int line)
{
    if (exp == ' ') {
        if (text[c] != ' ') {
            fprintf(fp, "%s:%d: expected a space in column %d.\n",
                    file, line + 1, c + 1);
            exit(EXIT_FAILURE);
        }
    }
    else if (exp != '0') {
        if (text[c] != exp) {
            fprintf(fp, "%s:%d: expected '%c' in column %d.\n",
                    file, line + 1, exp, c + 1);
            exit(EXIT_FAILURE);
        }
    }
}

static int load_sudoku(const char *file, Sudoku *sudoku)
{
    FILE *fp;
    int line;

    if ((fp = fopen(file, "r")) == NULL) {
        perror(file);
        return 1;
    }

    for (line = 0; line < 13; line++) {
        char text[32];

        if (fgets(text, sizeof(text), fp) == NULL) {
            if (feof(fp)) {
                fprintf(stderr, "%s: premature end of file.\n", file);
            }
            else {
                perror(file);
            }
            return 1;
        }
        else if (line % 4 == 0) {
            if (strcmp(text, "+-------+-------+-------+\n") != 0) {
                fprintf(stderr, "%s:%d: format error.\n", file, line + 1);
                return 1;
            }
        }
        else {
            int c = 0;
            int row_num = line * 3 / 4;

            for (int sqr_num = 0; sqr_num < 3; sqr_num++) {
                expect(text, c++, '|', stderr, file, line);

                for (int sqr_col = 0; sqr_col < 3; sqr_col++) {
                    expect(text, c++, ' ', stderr, file, line);
                    expect(text, c, '0', stderr, file, line);

                    int col_num = sqr_col + 3 * sqr_num;

                    if (isdigit(text[c])) {
                        sudoku->cell[row_num][col_num] = text[c] - '0';
                    }
                    else {
                        sudoku->cell[row_num][col_num] = 0;
                    }

                    c++;
                }

                expect(text, c++, ' ', stderr, file, line);
            }

            expect(text, c++, '|', stderr, file, line);
        }
    }

    fclose(fp);

    init_sudoku(sudoku);

    return 0;
}

int main(int argc, char *argv[])
{
    Sudoku sudoku;

    if (argc == 1) {
        usage(argv[0], EXIT_SUCCESS);
    }
    else if (load_sudoku(argv[1], &sudoku) != 0) {
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Input:\n"); dump_sudoku(stdout, &sudoku);

#if DEBUG == 1
    fprintf(stderr, "\033[H\033[2J");
#endif

    // Here we go!

    if (step1(&sudoku)) {
        fprintf(stderr, "Found a solution using method 1.\n");
    }
    else if (step2(&sudoku)) {
        fprintf(stderr, "Found a solution using method 2.\n");
    }
    else {
        fprintf(stderr, "Could not find a solution.\n");
    }

    return 0;
}
