.text
.globl palette_test_invoke
.type palette_test_invoke, @function
palette_test_invoke:
    /* Forward the argument without saving callee-saved registers: the
     * outer checker must see any register damage in the tested function. */
    pushl palette_test_depth
    call *palette_test_function
    addl $4, %esp
    ret
.size palette_test_invoke, .-palette_test_invoke
.section .note.GNU-stack,"",@progbits
