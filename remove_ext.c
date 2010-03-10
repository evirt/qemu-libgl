#include <stdlib.h>
#include <string.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

void removeUnwantedExtensions(char* ret)
{
  char* toBeRemoved = getenv("GL_REMOVE_EXTENSIONS");
  if (toBeRemoved == NULL) return;
  toBeRemoved = strdup(toBeRemoved);
  char* iterToBeRemoved = toBeRemoved;
  while(*iterToBeRemoved)
  {
    char* cSpace = strchr(iterToBeRemoved, ' ');
    char* cComma = strchr(iterToBeRemoved, ',');
    char* c = (cSpace && cComma) ? MIN(cSpace, cComma) : (cSpace) ? cSpace : (cComma) ? cComma : NULL;
    if (c != NULL)
    {
      *c = 0;
    }
    if (!(*iterToBeRemoved == ' ' || *iterToBeRemoved == ',' || *iterToBeRemoved == 0))
    {
      log_gl("Trying to remove : %s (%d)\n", iterToBeRemoved, *iterToBeRemoved);
      char* c2 = strstr(ret, iterToBeRemoved);
      while (c2)
      {
        memset(c2, 'X', strlen(iterToBeRemoved));
        c2 = strstr(c2 + strlen(iterToBeRemoved), iterToBeRemoved);
      }
    }
    if (c == NULL)
      break;
    iterToBeRemoved = c + 1;
  }
  free(toBeRemoved);
}

