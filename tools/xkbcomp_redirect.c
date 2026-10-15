/*
 * xkbcomp_redirect.c —— LD_PRELOAD 垫片（shim）
 *
 * 背景：Xvfb/X server 通过 Popen("/bin/sh -c \"...\"") 调用**硬编码**的
 *       `/usr/bin/xkbcomp` 来编译键盘映射。在「只读 /usr」的沙箱里，
 *       /usr/bin 不可写、系统也没装 xkbcomp，导致 Xvfb 起不来：
 *       `sh: line 1: /usr/bin/xkbcomp: No such file or directory`。
 *
 * 方案：用 LD_PRELOAD 注入到 Xvfb（及其 fork 出的 /bin/sh）中，拦截
 *       execve/execv/execvp/posix_spawn/posix_spawnp，把可执行路径
 *       `/usr/bin/xkbcomp` 透明重定向到 XKBCOMP_REAL 指定的真实路径
 *       （默认 /workspace/sysroot/usr/bin/xkbcomp）。
 *
 * 这样 X 服务器即可正常初始化 XKB 键盘，无需写 /usr、无需 CAP_SYS_ADMIN。
 *
 * 编译：
 *   gcc -shared -fPIC -O2 -o xkbcomp_redirect.so xkbcomp_redirect.c -ldl
 * 使用：
 *   LD_PRELOAD=/path/xkbcomp_redirect.so XKBCOMP_REAL=/workspace/sysroot/usr/bin/xkbcomp Xvfb ...
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <spawn.h>

/* 被拦截的“假”路径（X server 硬编码） */
static const char *FAKE_PATH = "/usr/bin/xkbcomp";

/* 真实 xkbcomp 路径，可由环境变量 XKBCOMP_REAL 覆盖 */
static const char *real_path(void)
{
    const char *p = getenv("XKBCOMP_REAL");
    return (p && *p) ? p : "/workspace/sysroot/usr/bin/xkbcomp";
}

/* 精确匹配 FAKE_PATH 才重定向，避免误伤其它程序 */
static const char *remap(const char *path)
{
    if (path && strcmp(path, FAKE_PATH) == 0)
        return real_path();
    return path;
}

typedef int (*execve_fn)(const char *, char *const[], char *const[]);
typedef int (*execv_fn)(const char *, char *const[]);
typedef int (*execvp_fn)(const char *, char *const[]);
typedef int (*posix_spawn_fn)(pid_t *, const char *,
                              const posix_spawn_file_actions_t *,
                              const posix_spawnattr_t *,
                              char *const[], char *const[]);
typedef int (*posix_spawnp_fn)(pid_t *, const char *,
                               const posix_spawn_file_actions_t *,
                               const posix_spawnattr_t *,
                               char *const[], char *const[]);

int execve(const char *path, char *const argv[], char *const envp[])
{
    static execve_fn real;
    if (!real) real = (execve_fn)dlsym(RTLD_NEXT, "execve");
    return real(remap(path), argv, envp);
}

int execv(const char *path, char *const argv[])
{
    static execv_fn real;
    if (!real) real = (execv_fn)dlsym(RTLD_NEXT, "execv");
    return real(remap(path), argv);
}

int execvp(const char *file, char *const argv[])
{
    static execvp_fn real;
    if (!real) real = (execvp_fn)dlsym(RTLD_NEXT, "execvp");
    return real(remap(file), argv);
}

int posix_spawn(pid_t *pid, const char *path,
                const posix_spawn_file_actions_t *fa,
                const posix_spawnattr_t *attr,
                char *const argv[], char *const envp[])
{
    static posix_spawn_fn real;
    if (!real) real = (posix_spawn_fn)dlsym(RTLD_NEXT, "posix_spawn");
    return real(pid, remap(path), fa, attr, argv, envp);
}

int posix_spawnp(pid_t *pid, const char *file,
                 const posix_spawn_file_actions_t *fa,
                 const posix_spawnattr_t *attr,
                 char *const argv[], char *const envp[])
{
    static posix_spawnp_fn real;
    if (!real) real = (posix_spawnp_fn)dlsym(RTLD_NEXT, "posix_spawnp");
    return real(pid, remap(file), fa, attr, argv, envp);
}
