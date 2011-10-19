" Vim syntax file for Coil
"
" Copyright (c) 2009 ITA Software, Inc.
" See LICENSE.txt for details.

" Add - and @ as valid word characters in addition to the defaults.
" Note that @ is a special char so it must be given as a range.
setlocal iskeyword=@,a-z,A-Z,48-57,@-@,-,_

" Comments
syn match coilComment "#.*$" contains=coilTodo,@Spell
syn keyword coilTodo TODO FIXME XXX contained

" Strings
syn match coilEscape +\\[nrt'"\\]+ contained
syn region coilString matchgroup=Normal start=+'+ end=+'+
    \ skip=+\\\\\|\\'+ oneline contains=coilEscape,@Spell
syn region coilString matchgroup=Normal start=+"+ end=+"+
    \ skip=+\\\\\|\\"+ oneline contains=coilEscape,@Spell
syn region coilString matchgroup=Normal start=+'''+ end=+'''+
    \ contains=coilEscape,@Spell
syn region coilString matchgroup=Normal start=+"""+ end=+"""+
    \ contains=coilEscape,@Spell

" Other values
syn keyword coilKeyword None True False
syn match coilNumber '\<-\?\d\+\(\.\d\+\)\?\>'

" Special attributes
syn match coilSpecial '\<@[a-z]\+\>'

" Set default high-light groups. Can be overridden later.
hi def link coilComment Comment
hi def link coilEscape  Special
hi def link coilTodo    Todo
hi def link coilString  String
hi def link coilKeyword Keyword
hi def link coilNumber  Number
hi def link coilSpecial Keyword

