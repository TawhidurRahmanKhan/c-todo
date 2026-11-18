#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_TASKS = 100, MAX_TITLE = 120 };

typedef struct {
    char title[MAX_TITLE + 1];
    int done;
} Task;

/* Returns 1 for a line, 0 for EOF, or -1 for an oversized line. */
static int read_line(const char *prompt, char *line, int size)
{
    fputs(prompt, stdout);
    fflush(stdout);
    if (!fgets(line, size, stdin))
        return 0;

    char *newline = strchr(line, '\n');
    if (newline) {
        *newline = '\0';
        return 1;
    }

    int ch = getchar();
    if (ch == '\n' || ch == EOF)
        return 1;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
    puts("Input is too long. Try again.");
    return -1;
}

static void list_tasks(const Task tasks[], int count)
{
    if (count == 0)
        puts("Your list is empty.");
    for (int i = 0; i < count; ++i)
        printf("%d. [%c] %s\n", i + 1, tasks[i].done ? 'x' : ' ', tasks[i].title);
}

int main(void)
{
    Task tasks[MAX_TASKS];
    int count = 0;
    char line[MAX_TITLE + 1];

    puts("C To-Do List");
    for (;;) {
        int result = read_line("\n1. List  2. Add  3. Toggle done  4. Delete  0. Quit\n> ",
                               line, sizeof line);
        if (result == 0 || (result == 1 && strcmp(line, "0") == 0))
            break;
        if (result < 0)
            continue;

        if (strcmp(line, "1") == 0) {
            list_tasks(tasks, count);
        } else if (strcmp(line, "2") == 0) {
            if (count == MAX_TASKS) {
                puts("Your list is full.");
                continue;
            }
            result = read_line("Task title: ", line, sizeof line);
            if (result == 0)
                break;
            if (result < 0)
                continue;
            if (line[strspn(line, " \t")] == '\0') {
                puts("Enter a task title.");
                continue;
            }
            strcpy(tasks[count].title, line);
            tasks[count].done = 0;
            ++count;
            puts("Task added.");
        } else if (strcmp(line, "3") == 0 || strcmp(line, "4") == 0) {
            int deleting = line[0] == '4';
            list_tasks(tasks, count);
            if (count == 0)
                continue;
            result = read_line("Task number: ", line, sizeof line);
            if (result == 0)
                break;
            if (result < 0)
                continue;

            char *end;
            errno = 0;
            long number = strtol(line, &end, 10);
            if (errno == ERANGE || *end != '\0' || number < 1 || number > count) {
                puts("Choose a number from the list.");
                continue;
            }
            int index = (int)number - 1;
            if (deleting) {
                for (int i = index; i + 1 < count; ++i)
                    tasks[i] = tasks[i + 1];
                --count;
                puts("Task deleted.");
            } else {
                tasks[index].done = !tasks[index].done;
                puts(tasks[index].done ? "Task completed." : "Task reopened.");
            }
        } else {
            puts("Choose an option from the menu.");
        }
    }
    return 0;
}
