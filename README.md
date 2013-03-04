toga-mp
=======

This is a fork of Toga II 1.2.1a, an old (by this point) chess program, which is itself a fork of Fruit, one of the most influential chess programs of the modern era. This was done around June 2007.

The fork was pretty much only for the purpose of implementing multithreaded search, which was absent in Fruit/Toga (and the more recent versions of Toga have a pretty lousy multithreaded search, last I checked). It utilizes the Young Brothers Wait algorithm.

I had hoped to finish the work in a weekend, which I mostly did IIRC. It worked fairly well, but there was some issue that prevented it from getting any sort of a speedup at all (it was much slower actually). I tried to fix it for a while, couldn't find the problem, and gave up. Perhaps this is of interest to somebody...
