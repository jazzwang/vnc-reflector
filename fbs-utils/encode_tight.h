void configure_tight_encoder(CARD32 *framebuffer,
                             CARD16 fb_width, CARD16 fb_height,
                             FBSOUT *fbk);

int num_rects_tight(FB_RECT *r);

int rfb_encode_tight(FB_RECT *r);
