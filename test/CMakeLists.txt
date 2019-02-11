add_executable(test_hdlc test_hdlc.c)
target_link_libraries(test_hdlc hdlc)
add_test(test_hdlc test_hdlc)
