/* Loads the built core and checks the archive contract it reports to the
 * frontend: ZIP is a content extension and archives reach the core
 * unextracted, so a multi-disk ZIP is mounted as a set. Exits non-zero
 * on failure. */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <libretro.h>

typedef void (*get_system_info_t)(struct retro_system_info *info);

static int has_ext(const char *list, const char *ext)
{
   size_t n = strlen(ext);
   const char *p = list;
   while (p && *p)
   {
      const char *end = strchr(p, '|');
      size_t len      = end ? (size_t)(end - p) : strlen(p);
      if (len == n && !strncmp(p, ext, n))
         return 1;
      p = end ? end + 1 : NULL;
   }
   return 0;
}

int main(int argc, char **argv)
{
   struct retro_system_info info;
   get_system_info_t get_info;
   int failures = 0;
#ifdef _WIN32
   HMODULE lib;
#else
   void *lib;
#endif

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s <core library>\n", argv[0]);
      return 2;
   }
#ifdef _WIN32
   lib      = LoadLibraryA(argv[1]);
   get_info = lib ? (get_system_info_t)GetProcAddress(lib, "retro_get_system_info") : NULL;
#else
   lib      = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
   get_info = lib ? (get_system_info_t)dlsym(lib, "retro_get_system_info") : NULL;
#endif
   if (!get_info)
   {
      fprintf(stderr, "sysinfo_test: cannot load retro_get_system_info from %s\n", argv[1]);
      return 2;
   }

   memset(&info, 0, sizeof(info));
   get_info(&info);

   if (!info.valid_extensions || !has_ext(info.valid_extensions, "zip"))
   {
      fprintf(stderr, "FAIL: zip is not a valid content extension\n");
      failures++;
   }
   if (!info.block_extract)
   {
      fprintf(stderr, "FAIL: block_extract is false, the frontend would unpack ZIPs\n");
      failures++;
   }
   if (!info.need_fullpath)
   {
      fprintf(stderr, "FAIL: need_fullpath is false\n");
      failures++;
   }

   if (failures)
      return 1;
   printf("sysinfo_test: all checks passed\n");
   return 0;
}
