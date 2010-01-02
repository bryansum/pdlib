To build and install pdlib as a static library for linking into other iPhone 
projects, I've created a shell script which kicks off the requisite Xcode builds
and creates a fat binary. This takes in a prefix argument. In short, to install 
pdlib to `$YOUR_PREFIX`:
  
    cd PdLib
    ./install.sh $YOUR_PREFIX

Then make sure to add both `$YOUR_PREFIX/include` to your project's Header Search 
Path as well as `$YOUR_PREFIX/lib` to the Library Search Path in addition to adding
$YOUR_PREFIX/lib/libpd.a to your current target's linked libraries. 
 
PdLibTest is dependent on liblo, which is included in the package. As building
fat static libraries for iPhoneOS is somewhat complicated, I made a quick install 
script that installs a fat binary of liblo for iPhone into `/usr/local`. This uses 
a `build_for_iphone` script written by [Christopher Stawarz](http://pseudogreen.org/)
PdLibTest is already preconfigured to search for the `/usr/local` header and library paths 
(`/include` and `/lib`, respectively). To install liblo for iPhone:

    cd liblo-0.26
    sudo ./quickinstall.sh 

