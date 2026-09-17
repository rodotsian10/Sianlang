#include <windows.h>
#include <gdiplus/gdiplus.h>
#include <objidl.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    ULONG_PTR token = 0;
    GdiplusStartupInput input = {0}; input.GdiplusVersion = 1;
    if (GdiplusStartup(&token, &input, NULL) != Ok) return 1;
    FILE *file = fopen("build/icon.rodot", "rb");
    if (!file) return 1;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *bytes = malloc((size_t)length);
    if (!bytes || fread(bytes, 1, (size_t)length, file) != (size_t)length) return 1;
    fclose(file);
    IStream *stream = SHCreateMemStream(bytes, (UINT)length);
    free(bytes);
    if (!stream) return 1;
    GpBitmap *bitmap = NULL;
    GpStatus status = GdipCreateBitmapFromStream(stream, &bitmap);
    UINT width = 0, height = 0;
    if (status == Ok) {
        GdipGetImageWidth((GpImage *)bitmap, &width);
        GdipGetImageHeight((GpImage *)bitmap, &height);
        GdipDisposeImage((GpImage *)bitmap);
    }
    stream->lpVtbl->Release(stream);
    GdiplusShutdown(token);
    printf("status=%d size=%ux%u\n", (int)status, width, height);
    return status == Ok && width == 128 && height == 128 ? 0 : 1;
}
