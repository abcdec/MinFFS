

g++ -g -mthreads -DHAVE_W32API_H -D__WXMSW__ -D__WXDEBUG__ -D_UNICODE -IC:\wxWidgets\lib\gcc_lib\mswud -IC:\wxWidgets\include -Wno-ctor-dtor-privacy -pipe -fmessage-length=0 -Wl,--subsystem,windows -mwindows -Wl,--enable-auto-import -c -o sanity.o sanity.cpp
g++ -o sanity.exe sanity.o -mthreads -LC:\wxWidgets\lib\gcc_lib -lwxmsw30ud_xrc -lwxmsw30ud_aui -lwxmsw30ud_html -lwxmsw30ud_adv -lwxmsw30ud_core -lwxbase30ud_xml -lwxbase30ud_net -lwxbase30ud -lwxtiffd -lwxjpegd -lwxpngd -lwxzlibd -lwxregexud -lwxexpatd -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lwxregexud -lwinspool -lwinmm -lshell32 -lcomctl32 -lole32 -loleaut32 -luuid -lrpcrt4 -ladvapi32 -lwsock32



