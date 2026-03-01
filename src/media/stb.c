// stb_image multimedia backend for notcurses -- images only, no video
// Written for the z/OS port where ffmpeg/oiio are not available
#include "builddef.h"
#ifdef USE_STB

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO  // we do our own file I/O for z/OS compat
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/visual-details.h"
#include "lib/internal.h"

// minimal details struct -- stb_image is stateless after load
typedef struct ncvisual_details {
  int placeholder; // C requires at least one member
} ncvisual_details;

static ncvisual_details*
stb_details_init(void){
  ncvisual_details* d = malloc(sizeof(*d));
  if(d){
    memset(d, 0, sizeof(*d));
  }
  return d;
}

static ncvisual*
stb_create(void){
  ncvisual* nc = malloc(sizeof(*nc));
  if(nc){
    memset(nc, 0, sizeof(*nc));
    if((nc->details = stb_details_init()) == NULL){
      free(nc);
      return NULL;
    }
  }
  return nc;
}

// read entire file into memory (needed because STBI_NO_STDIO)
static unsigned char*
read_file_to_mem(const char* filename, int* out_len){
  FILE* f = fopen(filename, "rb");
  if(!f){
    fprintf(stderr, "stb: fopen(%s) failed\n", filename);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  if(len <= 0){
    fprintf(stderr, "stb: ftell returned %ld for %s\n", len, filename);
    fclose(f);
    return NULL;
  }
  fseek(f, 0, SEEK_SET);
  unsigned char* buf = malloc((size_t)len);
  if(!buf){
    fclose(f);
    return NULL;
  }
  size_t rd = fread(buf, 1, (size_t)len, f);
  fclose(f);
  if((long)rd != len){
    fprintf(stderr, "stb: short read %zu/%ld for %s\n", rd, len, filename);
    free(buf);
    return NULL;
  }
  *out_len = (int)len;
  return buf;
}

static ncvisual*
stb_from_file(const char* filename){
  ncvisual* ncv = stb_create();
  if(ncv == NULL){
    fprintf(stderr, "stb_from_file: stb_create failed\n");
    return NULL;
  }
  int flen = 0;
  unsigned char* fbuf = read_file_to_mem(filename, &flen);
  if(!fbuf){
    ncvisual_destroy(ncv);
    return NULL;
  }
  int x, y, channels;
  // request 4 channels (RGBA) regardless of source format
  unsigned char* pixels = stbi_load_from_memory(fbuf, flen, &x, &y, &channels, 4);
  free(fbuf);
  if(pixels == NULL){
    fprintf(stderr, "stb: stbi_load failed for %s: %s\n", filename, stbi_failure_reason());
    ncvisual_destroy(ncv);
    return NULL;
  }
  ncv->pixx = x;
  ncv->pixy = y;
  ncv->rowstride = x * 4;
  ncvisual_set_data(ncv, pixels, true);
  return ncv;
}

static void
stb_details_seed(ncvisual* ncv){
  (void)ncv; // nothing to sync -- stb has no frame state
}

// single-frame images: first decode returns the already-loaded data,
// subsequent calls return EOF (1)
static int
stb_decode(ncvisual* ncv){
  if(ncv->data){
    return 0;  // data already loaded from stb_from_file
  }
  return -1;
}

static int
stb_decode_loop(ncvisual* ncv){
  int r = stb_decode(ncv);
  if(r == 1){
    return 0; // loop back -- but we're single-frame, just return success
  }
  return r;
}

// rows/cols: target output geometry (pixels)
static int
stb_blit(const ncvisual* ncv, unsigned rows, unsigned cols, ncplane* n,
         const struct blitset* bset, const blitterargs* bargs){
  int stride = ncv->rowstride;
  const void* data = ncv->data;
  void* scaled = NULL;
  if(ncv->pixy != rows || ncv->pixx != cols){
    size_t dstride = cols * sizeof(uint32_t);
    scaled = resize_bitmap(ncv->data, ncv->pixy, ncv->pixx,
                           ncv->rowstride, rows, cols, dstride);
    if(scaled == NULL){
      fprintf(stderr, "stb_blit: resize failed %ux%u -> %ux%u\n", ncv->pixy, ncv->pixx, rows, cols);
      return -1;
    }
    data = scaled;
    stride = (int)dstride;
  }
  int ret = 0;
  if(rgba_blit_dispatch(n, bset, stride, data, rows, cols, bargs) < 0){
    fprintf(stderr, "stb_blit: rgba_blit_dispatch failed\n");
    ret = -1;
  }
  free(scaled); // NULL-safe
  return ret;
}

static int
stb_resize(ncvisual* ncv, unsigned rows, unsigned cols){
  size_t dstride = cols * sizeof(uint32_t);
  uint32_t* scaled = resize_bitmap(ncv->data, ncv->pixy, ncv->pixx,
                                   ncv->rowstride, rows, cols, dstride);
  if(scaled == NULL){
    return -1;
  }
  ncvisual_set_data(ncv, scaled, true);
  ncv->pixy = rows;
  ncv->pixx = cols;
  ncv->rowstride = (unsigned)dstride;
  return 0;
}

static int
stb_stream(notcurses* nc, ncvisual* ncv, float timescale,
           ncstreamcb streamer, const struct ncvisual_options* vopts,
           void* curry){
  (void)nc; (void)ncv; (void)timescale;
  (void)streamer; (void)vopts; (void)curry;
  return -1; // no video streaming support
}

static ncplane*
stb_subtitle(ncplane* parent, const ncvisual* ncv){
  (void)parent; (void)ncv;
  return NULL; // no subtitle support
}

static void
stb_destroy(ncvisual* ncv){
  if(ncv){
    free(ncv->details);
    if(ncv->owndata){
      // stbi_load uses malloc, so free() is correct here
      free(ncv->data);
    }
    free(ncv);
  }
}

static int
stb_init(int loglevel){
  (void)loglevel;
  return 0; // nothing to initialize
}

static void
stb_printbanner(fbuf* f){
  fbuf_puts(f, "stb_image (images only)" NL);
}

ncvisual_implementation local_visual_implementation = {
  .visual_init = stb_init,
  .visual_printbanner = stb_printbanner,
  .visual_blit = stb_blit,
  .visual_create = stb_create,
  .visual_from_file = stb_from_file,
  .visual_details_seed = stb_details_seed,
  .visual_decode = stb_decode,
  .visual_decode_loop = stb_decode_loop,
  .visual_stream = stb_stream,
  .visual_subtitle = stb_subtitle,
  .visual_resize = stb_resize,
  .visual_destroy = stb_destroy,
  .rowalign = 0, // no padding requirement
  .canopen_images = true,
  .canopen_videos = false,
};

#endif // USE_STB
