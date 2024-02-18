
CFLAGS_CDCG = -g -O0 -Wall -Werror -D_GNU_SOURCE \
              -Wno-unused-variable \
              -Wno-unused-but-set-variable \
              -Wno-unused-result \
              -Wno-unused-function \
              -Wno-unused-value \
              -Wno-pointer-to-int-cast \
              -Wno-int-to-pointer-cast \
              -Wno-unused-label \
              -Wno-strict-aliasing \
              -Wno-format \
              -lm
                
CPPFLAGS_CDCG = -g -O0 -Wall -Werror -D_GNU_SOURCE \
                -Wno-unused-variable \
                -Wno-unused-but-set-variable \
                -Wno-unused-value \
                -Wno-unused-result \
                -Wno-formati \
                -lm

LDFLAGS_CDCG = -Wl,--no-undefined -avoid-version

