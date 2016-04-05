m4_divert(-1)
# foreach(x, (item_1, item_2, ..., item_n), stmt)
m4_define(`foreach', `m4_pushdef(`$1', `')_foreach(`$1', `$2', `$3')m4_popdef(`$1')')
m4_define(`_arg1', `$1')
m4_define(`_foreach',
`m4_ifelse(`$2', `()', ,
`m4_define(`$1', _arg1$2)$3`'_foreach(`$1', (m4_shift$2), `$3')')')
m4_divert(0)
