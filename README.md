# C To-Do List

A small to-do list written in C for Windows, in two versions:

- **Web version** (`web.c` → `todo-web.exe`): runs a tiny local web server and opens the list in your browser at http://127.0.0.1:8080. Add tasks, tick them off, and delete them. Tasks are saved to `tasks.txt` next to the program.
- **Terminal version** (`main.c` → `todo.exe`): a numbered text menu. Tasks are kept in memory for the current run only.

The web version only listens on your own computer, so other devices can't reach it.

## Build

You need the Microsoft C Build Tools (Visual Studio Build Tools with the C++ workload). From this folder, run:

```powershell
.\build.cmd
```

## Run

```powershell
.\todo-web.exe   # browser version; close its window to stop it
.\todo.exe       # terminal version
```

You can also double-click either `.exe` in File Explorer.
