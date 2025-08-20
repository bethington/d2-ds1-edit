This program use the Allegro Library. I used the version 4.4.2. Check http://www.allegro.cc/files/?v=4.4

It was compiled with Microsoft Microsoft Visual C++ 2010.

The mpq\ directory is my modification of the 'Sample Console App source code'
at http://www.angelfire.com/sc/mpq/ Big thanks to Tom Amigo to have released
these sources. It's a really usefull piece of code. I needed very few minor
modifications to suit my needs.

To compile the editor, you need to make a new project with all .c and all .h,
and don't forget to link to the Allegro library.

With MS Visual, important points :

   * file / new / projects / win32 console application
   * platforms : win32
   * an empty project

   * add all .c and .h, and don't forget the ones in mpq\ directory

   * project / settings, tab 'link', in object/library modules,
     add the proper allegro .lib that fit your need (I used allegro-4.4.2-monolith-md.lib) 

You shouldn't have any warning or error.
