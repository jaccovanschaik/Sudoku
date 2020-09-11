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
 * square. Using bit masks enables us to do bitwise ANDs to find out which
 * numbers are still available for a particular cell.
 */

typedef struct {
    uint16_t row[9];        /* Available numbers for each row. */
    uint16_t col[9];        /* Available numbers for each column. */
    uint16_t sqr[3][3];     /* Available numbers for each 3x3 square. */
    uint8_t  cell[9][9];    /* Contents of each cell (0 means empty). */
} Sudoku;

/* The bit mask that represents a value in a cell. */

#define BIT_FOR_VALUE(val) (1 << ((val) - 1))

// #define DEBUG

/*
 * Dump the contents of sudoku grid <sudoku> to <fp>.
 */
void dump(FILE *fp, const Sudoku *sudoku)
{
    int row, col;

    for (row = 0; row < 9; row++) {
        if ((row % 3) == 0) {
            fprintf(fp, "+-----+-----+-----+\n");
        }
        for (col = 0; col < 9; col++) {
            if ((col % 3) == 0) {
                fprintf(fp, "|");
            }

            if ((col % 3) != 0) fprintf(fp, " ");

            if (sudoku->cell[row][col] == 0)
                fprintf(fp, " ");
            else
                fprintf(fp, "%hhu", sudoku->cell[row][col]);

            if (col == 8) fprintf(fp, "|\n");
        }

        if (row == 8) {
            fprintf(fp, "+-----+-----+-----+\n");
        }
    }
}

/*
 * Find the next empty cell in <sudoku>. If found, returns 1 and fills <row> and
 * <col> with its location. Otherwise returns 0.
 */
int find_empty_cell(const Sudoku *sudoku, int *row, int *col)
{
    for (*row = 0; *row < 9; (*row)++) {
        for (*col = 0; *col < 9; (*col)++) {
            if (sudoku->cell[*row][*col] == 0) return 1;
        }
    }

    return 0;
}

/*
 * Copy <orig> to new, fill the cell at row <row> and column <col> with <value>
 * and update the bitmasks that represent the now available values for the row,
 * column and square that this cell is in.
 */
void fill_cell(const Sudoku *orig, Sudoku *new, int row, int col, int value)
{
    /* Copy the current sudoku... */
    memcpy(new, orig, sizeof(Sudoku));

    /* Fill the current cell... */
    new->cell[row][col] = value;

    /* Update the available values for this row, column and square. */
    new->row[row]          &= ~(BIT_FOR_VALUE(value));
    new->col[col]          &= ~(BIT_FOR_VALUE(value));
    new->sqr[row/3][col/3] &= ~(BIT_FOR_VALUE(value));
}

#ifdef DEBUG
/*
 * Dump the bit mask of available values in <avail> to <fp>, preceded by
 * <intro>.
 */
void dump_availables(FILE *fp, const char *intro, uint16_t avail)
{
    int val;

    fprintf(fp, "\033[K%s:", intro);

    for (val = 1; val <= 9; val++) {
        if (avail & (BIT_FOR_VALUE(val))) fprintf(fp, " %d", val);
    }

    fprintf(fp, "\n");
}
#endif

/*
 * Method 1. Find any cells where only one value is possible, fill it with that
 * value and then recursively call myself to try and solve this new sudoku.
 * Returns 1 if successful, 0 if not.
 */
int solve1(const Sudoku *sudoku)
{
    /*
     * An array to help us identify if a cell has only one available value. The
     * index is a bitmask of available values, the value is either 0 if there is
     * more than one (or no!) available value, otherwise it is the only
     * available value. The number of possible bit masks is 2^9.
     */
    const static int only_possible_value[1 << 9] = {
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

    if (!find_empty_cell(sudoku, &row, &col)) {
        /* No empty cells left. We're done! */

        fprintf(stdout, "Solution:\n"); dump(stdout, sudoku);

        return 1;
    }

    while (1) {
        uint16_t available_in_row = sudoku->row[row];
        uint16_t available_in_col = sudoku->col[col];
        uint16_t available_in_sqr = sudoku->sqr[row/3][col/3];
        uint16_t available_values = available_in_row
                                  & available_in_col
                                  & available_in_sqr;

        int the_only_possible_value = only_possible_value[available_values];

        if (sudoku->cell[row][col] == 0 && the_only_possible_value != 0) {
            Sudoku new_sudoku;

            // Clone this sudoku with this cell set to <the_only_possible_value>
            fill_cell(sudoku, &new_sudoku, row, col, the_only_possible_value);

            // And try to solve this new sudoku.
            if (solve1(&new_sudoku)) {
                return 1; // Solution found. We're done! */
            }
        }

        if ((col = col + 1) < 9) {
            continue;
        }
        else if ((row = row + 1) < 9) {
            col = 0;
            continue;
        }
        else {
            break;
        }
    }

    return 0;
}

/*
 * Method 2. Fill the first empty cell with all possible values in turn, and
 * call myself recursively each time to try and solve that new sudoku. Returns 1
 * if successful, 0 if not.
 */
int solve2(const Sudoku *sudoku)
{
    int value, row, col;

    if (!find_empty_cell(sudoku, &row, &col)) {
        /* No empty cells left. We're done! */

        fprintf(stdout, "Solution:\n"); dump(stdout, sudoku);

        return 1;
    }

    /* Find out which values are available for this cell. */

    uint16_t available_in_row = sudoku->row[row];
    uint16_t available_in_col = sudoku->col[col];
    uint16_t available_in_sqr = sudoku->sqr[row/3][col/3];
    uint16_t available_values = available_in_row & available_in_col & available_in_sqr;

    if (available_values == 0) {
        return 0;
    }

#ifdef DEBUG
    fprintf(stderr, "\033[H");

    dump(stderr, sudoku);

    usleep(10000);

    fprintf(stderr, "row %d, col %d:\n", row + 1, col + 1);

    dump_availables(stderr, "\tavailable_in_row", available_in_row);
    dump_availables(stderr, "\tavailable_in_col", available_in_col);
    dump_availables(stderr, "\tavailable_in_sqr", available_in_sqr);
    dump_availables(stderr, "\tavailable_values", available_values);
#endif

    /* Now fill this cell with each of the available values in turn, and see if
     * we can recursively solve2 the rest of the sudoku. */

    for (value = 1; value <= 9; value++) {
        /* If <value> is available... */
        if (available_values & (BIT_FOR_VALUE(value))) {
            Sudoku new_sudoku;

            /* Make a new sudoku with this cell set... */
            fill_cell(sudoku, &new_sudoku, row, col, value);

            if (solve2(&new_sudoku)) {
                /* Solution found, somewhere down the line. We're done! */

                return 1;
            }
        }
    }

    /* No solution found. */

    return 0;
}

/*
 * Give some usage info for <argv0> and exit with exit code <exitcode>.
 */
void usage(const char *argv0, int exitcode)
{
    fprintf(stderr, "Usage: %s <sudoku_input_file>\n", basename(argv0));

    exit(exitcode);
}

int main(int argc, char *argv[])
{
    Sudoku sudoku;
    int row = 0, col = 0;
    char line[11];
    FILE *fp;

    if (argc == 1) usage(argv[0], 0);

    if ((fp = fopen(argv[1], "r")) == NULL) {
        perror(argv[1]);
        usage(argv[0], 1);
    }

    memset(&sudoku, 0, sizeof(sudoku));

    /* Read input. */

    for (row = 0; row < 9; row++) {
        if (fgets(line, 10, fp) == NULL) break;

        for (col = 0; col < 9; col++) {
            if (line[col] == '\n') {
                break;
            }
            else if (isdigit(line[col])) {
                sudoku.cell[row][col] = line[col] - 48;
            }
        }

        if (line[strlen(line) - 1] != '\n') while (fgetc(fp) != '\n');
    }

    fclose(fp);

    fprintf(stdout, "Input:\n"); dump(stdout, &sudoku);

    /* First mark all rows, columns and squares as "all values available". */

    for (row = 0; row < 9; row++) sudoku.row[row] = 0x1FF; /* bits 1-9. */
    for (col = 0; col < 9; col++) sudoku.col[col] = 0x1FF;

    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++) {
            sudoku.sqr[row][col] = 0x1FF;
        }
    }

    /* Now unmark the values for all cells that we know. */

    for (row = 0; row < 9; row++) {
        for (col = 0; col < 9; col++) {
            uint8_t value = sudoku.cell[row][col];

            if (value == 0) continue;

            uint8_t sq_row = row / 3;
            uint8_t sq_col = col / 3;

            sudoku.row[row]            &= ~(BIT_FOR_VALUE(value));
            sudoku.col[col]            &= ~(BIT_FOR_VALUE(value));
            sudoku.sqr[sq_row][sq_col] &= ~(BIT_FOR_VALUE(value));
        }
    }

#ifdef DEBUG
    fprintf(stderr, "\033[H\033[2J");
#endif

    /* Here we go! */

    if (solve1(&sudoku)) {
        fprintf(stderr, "Found a solution using method 1.\n");
    }
    else if (solve2(&sudoku)) {
        fprintf(stderr, "Found a solution using method 2.\n");
    }
    else {
        fprintf(stderr, "Could not find a solution.\n");
    }

    return 0;
}
