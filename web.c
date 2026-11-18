#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

enum { MAX_TASKS = 100, MAX_TITLE = 120, PORT = 8080 };

typedef struct {
    char title[MAX_TITLE + 1];
    int done;
} Task;

static Task tasks[MAX_TASKS];
static int count;
static const char *SAVE_FILE = "tasks.txt";

static const char PAGE[] =
    "<!doctype html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'><title>To-Do List</title><style>"
    ":root{--bg:#f5f5f4;--card:#fff;--text:#1c1917;--muted:#78716c;--line:#e7e5e4;--accent:#2563eb}"
    "@media(prefers-color-scheme:dark){:root{--bg:#1c1917;--card:#292524;--text:#f5f5f4;--muted:#a8a29e;--line:#44403c;--accent:#3b82f6}}"
    "body{margin:0;background:var(--bg);color:var(--text);font:16px system-ui,sans-serif}"
    "main{max-width:560px;margin:48px auto;padding:0 16px}"
    "h1{font-size:28px;margin:0 0 4px}#sum{color:var(--muted);margin:0 0 24px;min-height:1.2em}"
    "form{display:flex;gap:8px;margin-bottom:16px}"
    "#t{flex:1;min-width:0;padding:10px 12px;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--text);font:inherit}"
    "button{padding:10px 16px;border:0;border-radius:8px;background:var(--accent);color:#fff;font:inherit;cursor:pointer}"
    "ul{list-style:none;margin:0;padding:0;background:var(--card);border:1px solid var(--line);border-radius:12px}"
    "li{display:flex;align-items:center;gap:12px;padding:12px 16px;border-top:1px solid var(--line)}li:first-child{border-top:0}"
    "li span{flex:1;word-break:break-word}li.done span{text-decoration:line-through;color:var(--muted)}"
    "li input{width:18px;height:18px;cursor:pointer}"
    ".del{background:none;color:var(--muted);padding:2px 8px;font-size:20px}.del:hover{color:#dc2626}"
    ".empty{justify-content:center;color:var(--muted);padding:24px}"
    "</style></head><body><main><h1>To-Do List</h1><p id=sum></p>"
    "<form id=f><input id=t maxlength=120 placeholder='What needs doing?' autocomplete=off autofocus><button>Add</button></form>"
    "<ul id=l></ul></main><script>"
    "function call(p,m,b){return fetch(p,{method:m,headers:{'X-Todo':'1'},body:b}).then(r=>r.json()).then(draw)}"
    "function draw(ts){const l=document.getElementById('l');l.innerHTML='';"
    "if(!ts.length)l.innerHTML='<li class=empty>Nothing to do yet.</li>';"
    "ts.forEach((t,i)=>{const li=document.createElement('li');if(t.done)li.className='done';"
    "const c=document.createElement('input');c.type='checkbox';c.checked=t.done;c.onchange=()=>call('/api/toggle?i='+i,'POST');"
    "const s=document.createElement('span');s.textContent=t.title;"
    "const d=document.createElement('button');d.className='del';d.innerHTML='&times;';d.title='Delete';"
    "d.onclick=()=>call('/api/delete?i='+i,'POST');li.append(c,s,d);l.append(li)});"
    "const n=ts.filter(t=>!t.done).length;"
    "document.getElementById('sum').textContent=ts.length?n+' of '+ts.length+' left':''}"
    "document.getElementById('f').onsubmit=e=>{e.preventDefault();const t=document.getElementById('t');"
    "if(!t.value.trim())return;call('/api/add','POST',t.value).then(()=>{t.value='';t.focus()})};"
    "call('/api/tasks','GET');"
    "</script></body></html>";

static void save_tasks(void)
{
    FILE *f = fopen(SAVE_FILE, "w");
    if (!f)
        return;
    for (int i = 0; i < count; ++i)
        fprintf(f, "%d|%s\n", tasks[i].done, tasks[i].title);
    fclose(f);
}

static void load_tasks(void)
{
    FILE *f = fopen(SAVE_FILE, "r");
    if (!f)
        return;
    char line[256];
    while (count < MAX_TASKS && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if ((line[0] == '0' || line[0] == '1') && line[1] == '|' && line[2]) {
            tasks[count].done = line[0] == '1';
            strncpy(tasks[count].title, line + 2, MAX_TITLE);
            tasks[count].title[MAX_TITLE] = '\0';
            ++count;
        }
    }
    fclose(f);
}

static void send_all(SOCKET s, const char *data, int len)
{
    while (len > 0) {
        int sent = send(s, data, len, 0);
        if (sent <= 0)
            return;
        data += sent;
        len -= sent;
    }
}

static void respond(SOCKET s, const char *status, const char *type, const char *body, int len)
{
    char head[256];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     status, type, len);
    send_all(s, head, n);
    send_all(s, body, len);
}

static void respond_tasks(SOCKET s)
{
    static char json[MAX_TASKS * (MAX_TITLE * 6 + 32) + 8];
    int n = 0;
    json[n++] = '[';
    for (int i = 0; i < count; ++i) {
        n += sprintf(json + n, "%s{\"done\":%s,\"title\":\"", i ? "," : "",
                     tasks[i].done ? "true" : "false");
        for (const unsigned char *p = (const unsigned char *)tasks[i].title; *p; ++p) {
            if (*p == '"' || *p == '\\') {
                json[n++] = '\\';
                json[n++] = (char)*p;
            } else if (*p < 0x20) {
                n += sprintf(json + n, "\\u%04x", *p);
            } else {
                json[n++] = (char)*p;
            }
        }
        json[n++] = '"';
        json[n++] = '}';
    }
    json[n++] = ']';
    respond(s, "200 OK", "application/json", json, n);
}

static int has_header(const char *req, const char *end, const char *name)
{
    size_t len = strlen(name);
    for (const char *p = req; p < end; ++p)
        if (_strnicmp(p, name, len) == 0)
            return 1;
    return 0;
}

static int parse_index(const char *text)
{
    char *end;
    long i = strtol(text, &end, 10);
    return (*end == '\0' && i >= 0 && i < count) ? (int)i : -1;
}

static void add_task(char *body)
{
    if (count == MAX_TASKS)
        return;
    for (char *p = body; *p; ++p)
        if ((unsigned char)*p < 0x20)
            *p = ' ';
    while (*body == ' ')
        ++body;
    size_t len = strlen(body);
    while (len > 0 && body[len - 1] == ' ')
        --len;
    if (len > MAX_TITLE) {
        len = MAX_TITLE;
        while (len > 0 && ((unsigned char)body[len] & 0xC0) == 0x80)
            --len;
    }
    if (len == 0)
        return;
    memcpy(tasks[count].title, body, len);
    tasks[count].title[len] = '\0';
    tasks[count].done = 0;
    ++count;
    save_tasks();
}

static void handle(SOCKET c)
{
    char req[8192];
    int len = 0;
    char *body = NULL;
    char *headers_end = NULL;
    long content_length = 0;

    while (len < (int)sizeof req - 1) {
        int r = recv(c, req + len, (int)sizeof req - 1 - len, 0);
        if (r <= 0)
            break;
        len += r;
        req[len] = '\0';
        if (!body) {
            headers_end = strstr(req, "\r\n\r\n");
            if (!headers_end)
                continue;
            body = headers_end + 4;
            for (char *p = req; p < headers_end; ++p)
                if (_strnicmp(p, "\r\nContent-Length:", 17) == 0) {
                    content_length = strtol(p + 17, NULL, 10);
                    break;
                }
        }
        if (req + len - body >= content_length)
            break;
    }
    if (!body)
        return;
    if (content_length >= 0 && content_length < req + len - body)
        body[content_length] = '\0';

    char method[8], path[256];
    if (sscanf(req, "%7s %255s", method, path) != 2)
        return;

    int is_post = strcmp(method, "POST") == 0;
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        respond(c, "200 OK", "text/html; charset=utf-8", PAGE, (int)(sizeof PAGE - 1));
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/tasks") == 0) {
        respond_tasks(c);
    } else if (is_post && !has_header(req, headers_end, "\r\nX-Todo:")) {
        respond(c, "403 Forbidden", "text/plain", "Forbidden", 9);
    } else if (is_post && strcmp(path, "/api/add") == 0) {
        add_task(body);
        respond_tasks(c);
    } else if (is_post && strncmp(path, "/api/toggle?i=", 14) == 0) {
        int i = parse_index(path + 14);
        if (i >= 0) {
            tasks[i].done = !tasks[i].done;
            save_tasks();
        }
        respond_tasks(c);
    } else if (is_post && strncmp(path, "/api/delete?i=", 14) == 0) {
        int i = parse_index(path + 14);
        if (i >= 0) {
            memmove(&tasks[i], &tasks[i + 1], (size_t)(count - i - 1) * sizeof tasks[0]);
            --count;
            save_tasks();
        }
        respond_tasks(c);
    } else {
        respond(c, "404 Not Found", "text/plain", "Not found", 9);
    }
}

int main(void)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        puts("Could not start networking.");
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (server == INVALID_SOCKET || bind(server, (struct sockaddr *)&addr, sizeof addr) != 0 ||
        listen(server, SOMAXCONN) != 0) {
        printf("Could not listen on port %d. Is the app already running?\n", PORT);
        WSACleanup();
        return 1;
    }

    load_tasks();
    printf("To-Do List running at http://127.0.0.1:%d\nClose this window to stop it.\n", PORT);
    ShellExecuteA(NULL, "open", "http://127.0.0.1:8080", NULL, NULL, SW_SHOWNORMAL);

    for (;;) {
        SOCKET client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET)
            continue;
        handle(client);
        closesocket(client);
    }
}
