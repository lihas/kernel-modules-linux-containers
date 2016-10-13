This folder will be uploaded online on github, and cse. Its links will be included in report. 

All CARE must be taken that links don't change ever, since the report cannot be modified afterwards.

#Tags
Tags represent some version of code. Whenever a significant feature will be added, a new tag will be created.
The floowing contains descriptions of each tag created so far.
###simple-assembly-modules-support
This version of code supports installing simple kernel, consisting of assembly codes modules from inside of a container. An example of such a module exists in **asm-module** directory.

#Patch
From the patch file **complete.patch** the complete directory can be recreated.
Here is how the patch was created, and how to use it.

##How the patch was created
1. Create a directory

```mkdir ../diff```

1. copy necessary files to it
```
for i in `find -iname "*.c" -o -iname "*.h" -o -iname "*.asm" -o -iname "*.rules" -o -iname "*.md" -o -iname "Makefile" -o -iname "*.patch"`
do              
cp --parents "$i" ../diff/
done

```

1. Move to that directory to create patch
```
cd ../diff
```

1. Create patch

```
for i in `find . -type f|tail -n +2`                                                                                                        
do                 
diff -c /dev/null "$i" >> complete.patch
done
```

##Apply patch
```
patch -p0 <complete.patch 
```
