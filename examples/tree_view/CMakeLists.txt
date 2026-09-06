huxerui_add_example(
        example_tree_view
        "HuxerUI File Tree"
        "com.huxerui.file-tree"
)

huxerui_add_resources(example_tree_view
        ROOT "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        NAMESPACE "tree_view"
)
