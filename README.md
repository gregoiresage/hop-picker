HopPicker's watchface
==========

This watchface is based on the HopPicker's concept below :

![Hop picker's concept](http://i.imgur.com/Bf4C7nZ.gif)


Clone instructions :
```
git clone --recursive https://github.com/gregoiresage/hop-picker.git
cd hop-picker
git submodule foreach -q --recursive 'branch="$(git config -f $toplevel/.gitmodules submodule.$name.branch)"; git checkout $branch'
```
