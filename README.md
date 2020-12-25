# LHDecompressor
`LH` (LZ77+Huffman) decompressor in C++.  
Decompiled from NSMBW and simplified by hand.  
  
Originally written in [Cython](https://github.com/H1dd3nM1nd/Reggie-Next/blob/6ce91a7e37b93461a5c3a1377f7b57000aecb8d0/libs/lz77_huffman_cy.pyx), then ported to [Python](https://github.com/H1dd3nM1nd/Reggie-Next/blob/6ce91a7e37b93461a5c3a1377f7b57000aecb8d0/libs/lz77_huffman.py) and C++.  
Previously influenced by the [`sead::SZSDecompressor` decompilation](https://github.com/open-ead/sead/blob/master/modules/src/resource/seadSZSDecompressor.cpp).  
  
This serves as an upgrade to the [original project by Treeki](https://github.com/Treeki/RandomStuff/blob/master/LHDecompressor.cpp), which was very unclear on the format and frequently broke.