#!/bin/bash

awk '/^==/{}/^==.*(definitely|possibly|indirectly) lost:/{if ($4 > 0) print}/^$/{print}/^[^=]/{print}'
