huxerui_add_example(example_local_notification "HuxerUI Local Notifications" "com.huxerui.localnotification")

if (IOS AND TARGET huxerui_static)
    # The extension links the SDK alone, without the app core's force-loaded Application declaration.
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/huxerui-ios/notification-extension-link.rsp"
            CONTENT "\"$<TARGET_FILE:huxerui_static>\"\n$<JOIN:$<TARGET_PROPERTY:huxerui_static,INTERFACE_LINK_LIBRARIES>,\n>\n-lc++\n-framework\nUserNotificationsUI\n")
endif ()
