# Usage Instructions #

Add git submodule to project:

```
#!bash
cd /project_folder
git submodule add git@bitbucket.org:crosswalkguy/ivy-qt.git ivy-qt
```


**Add following to .pro file**


```
#!c++

#INCLUDEPATH += "ivy-qt/"
include(ivy-qt/ivy-qt.pri)
```


When cloning parent projects, from parent (root) folder of project:

```
git submodule init
git submodule update
```