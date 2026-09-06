#include <huxerui/huxerui.h>

#include <tree_view_resources.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace huxerui;
using namespace std::chrono_literals;

enum class ChildrenState { Loaded, Deferred, Loading };

struct FileNode;
using Node = std::shared_ptr<FileNode>;

struct FileNode {
  std::string name;
  std::string path;
  bool directory = false;
  bool expanded = false;
  ChildrenState children_state = ChildrenState::Loaded;
  std::vector<Node> children;
};

struct FileTreeModel {
  std::vector<Node> roots;
  Node selected;
  Node activated;
};

Color WithOpacity(Color color, float opacity) {
  color.alpha *= opacity;
  return color;
}

Node MakeFile(std::string name, std::string path) {
  return std::make_shared<FileNode>(FileNode{.name = std::move(name), .path = std::move(path)});
}

Node MakeDirectory(std::string name, std::string path, std::vector<Node> children = {}, bool expanded = false,
                   ChildrenState children_state = ChildrenState::Loaded) {
  return std::make_shared<FileNode>(FileNode{
      .name = std::move(name),
      .path = std::move(path),
      .directory = true,
      .expanded = expanded,
      .children_state = children_state,
      .children = std::move(children),
  });
}

FileTreeModel CreateFileTree() {
  return {
      .roots = {
          MakeDirectory(
              "HuxerUI", "HuxerUI",
              {
                  MakeDirectory(
                      "include", "HuxerUI/include",
                      {
                          MakeDirectory("huxerui", "HuxerUI/include/huxerui",
                                        {MakeFile("view.h", "HuxerUI/include/huxerui/view.h"),
                                         MakeFile("theme.h", "HuxerUI/include/huxerui/theme.h")},
                                        true),
                      },
                      true),
                  MakeDirectory(
                      "src", "HuxerUI/src",
                      {
                          MakeDirectory("components", "HuxerUI/src/components",
                                        {MakeFile("tree.cpp", "HuxerUI/src/components/tree.cpp")}, true),
                          MakeDirectory("runtime", "HuxerUI/src/runtime",
                                        {MakeFile("semantics.cpp", "HuxerUI/src/runtime/semantics.cpp")}),
                      },
                      true),
                  MakeDirectory("examples", "HuxerUI/examples", {}, false, ChildrenState::Deferred),
                  MakeFile("CMakeLists.txt", "HuxerUI/CMakeLists.txt"),
                  MakeFile("README.md", "HuxerUI/README.md"),
              },
              true),
      },
  };
}

std::vector<Node> LoadedExamples() {
  return {
      MakeDirectory("tree_view", "HuxerUI/examples/tree_view",
                    {MakeFile("main.cpp", "HuxerUI/examples/tree_view/main.cpp"),
                     MakeFile("CMakeLists.txt", "HuxerUI/examples/tree_view/CMakeLists.txt")}),
      MakeDirectory("ui_gallery", "HuxerUI/examples/ui_gallery",
                    {MakeFile("main.cpp", "HuxerUI/examples/ui_gallery/main.cpp")}),
      MakeDirectory("virtual_list", "HuxerUI/examples/virtual_list",
                    {MakeFile("main.cpp", "HuxerUI/examples/virtual_list/main.cpp")}),
  };
}

std::size_t CountNodes(const std::vector<Node>& nodes) {
  std::size_t count = nodes.size();
  for (const Node& node : nodes) {
    count += CountNodes(node->children);
  }
  return count;
}

bool IsLoading(const std::vector<Node>& nodes) {
  for (const Node& node : nodes) {
    if (node->children_state == ChildrenState::Loading || IsLoading(node->children)) {
      return true;
    }
  }
  return false;
}

View FileIcon(bool directory, const ThemeSpec& theme, float extent = 18.0F) {
  const ImageVariant icon = directory ? ImageVariant{tree_view::images::folder} : ImageVariant{tree_view::images::file};
  const Color accent = directory ? theme.colors.primary : theme.colors.on_surface_variant;
  return Image(std::move(icon))
      .Fit(ImageFit::Contain)
      .Tint(accent)
      .With(Frame{.width = extent, .height = extent}, Semantics{.hidden = true});
}

View Badge(std::string label, Color foreground, Color background) {
  return Text(std::move(label), TextRole::Label)
      .Style({Font::System(11.0F).WithWeight(FontWeight::SemiBold), foreground})
      .With(Padding(EdgeInsets::Symmetric(9.0F, 4.0F)), Background(background), CornerRadius(100.0F));
}

[[huxerui::composable]]
View FileRow(Node node, ChildrenState children_state) {
  const ThemeSpec& theme = UseTheme();
  View status = children_state == ChildrenState::Loading
      ? ProgressCircle().With(Frame{.width = 16.0F, .height = 16.0F}, Semantics{.hidden = true})
      : Spacer().With(Frame{.width = 16.0F});

  return Row {
    FileIcon(node->directory, theme),
    Text(node->name).Style({Font::System(14.0F).WithWeight(FontWeight::Regular), theme.colors.on_surface}),
    Spacer(),
    std::move(status),
  }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center));
}

View ProjectTreeCard(View tree, std::size_t item_count, bool loading, const ThemeSpec& theme) {
  return Column {
    Row {
      Column {
        FileIcon(true, theme, 20.0F),
      }.With(
          Frame{.width = 38.0F, .height = 38.0F},
          Background(WithOpacity(theme.colors.primary, 0.10F)),
          CornerRadius(10.0F),
          MainAlign(MainAxisAlignment::Center),
          CrossAlign(CrossAxisAlignment::Center)
      ),
      Column {
        Text("HuxerUI").Style({Font::System(16.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
        Text::Format("{} project items", item_count)
            .Style({Font::System(12.0F), theme.colors.on_surface_variant}),
      }.With(Spacing(2.0F)),
      Spacer(),
      loading
          ? Badge("LOADING", theme.colors.primary, WithOpacity(theme.colors.primary, 0.10F))
          : Badge("READY", theme.colors.on_primary_container, theme.colors.primary_container),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
    Spacer().With(Frame{.height = 1.0F}, Background(WithOpacity(theme.colors.outline, 0.16F))),
    std::move(tree),
    Row {
      Badge("INPUT", theme.colors.on_surface_variant, theme.colors.surface_container_high),
      Text("Double-click on desktop, tap on touch")
          .Style({Font::System(12.0F), theme.colors.on_surface_variant}),
    }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
  }.With(
      Frame{.min_width = 280.0F, .max_width = 560.0F},
      Padding(18.0F),
      Spacing(14.0F),
      Background(theme.colors.surface),
      Border{WithOpacity(theme.colors.outline, 0.20F), 1.0F},
      CornerRadius(18.0F),
      Shadow{
          .color = WithOpacity(theme.colors.scrim, 0.12F),
          .offset = {0.0F, 6.0F},
          .blur_radius = 22.0F,
          .spread = -7.0F,
      },
      CrossAlign(CrossAxisAlignment::Stretch));
}

View DetailField(std::string label, std::string value, const ThemeSpec& theme) {
  return Column {
    Text(std::move(label), TextRole::Label)
        .Style({Font::System(11.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface_variant}),
    SelectionArea {
      Text(std::move(value)).Style({Font::System(14.0F), theme.colors.on_surface}),
    },
  }.With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Stretch));
}

View InspectorCard(Node selected, Node activated, const ThemeSpec& theme) {
  const bool has_selection = static_cast<bool>(selected);
  const std::string name = has_selection ? selected->name : "Nothing selected";
  const std::string type = has_selection ? (selected->directory ? "Directory" : "File") : "None";
  const std::string path = has_selection ? selected->path : "Select a row in the project tree";
  const std::string activated_name = activated ? activated->name : "None yet";

  return Column {
    Row {
      Column {
        FileIcon(has_selection && selected->directory, theme, 24.0F),
      }.With(
          Frame{.width = 46.0F, .height = 46.0F},
          Background(theme.colors.surface_container_high),
          CornerRadius(12.0F),
          MainAlign(MainAxisAlignment::Center),
          CrossAlign(CrossAxisAlignment::Center)
      ),
      Column {
        Text("INSPECTOR", TextRole::Label)
            .Style({Font::System(11.0F).WithWeight(FontWeight::SemiBold), theme.colors.primary}),
        Text(name).Style({Font::System(18.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
      }.With(Spacing(3.0F)),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)),
    Spacer().With(Frame{.height = 1.0F}, Background(WithOpacity(theme.colors.outline, 0.16F))),
    DetailField("TYPE", type, theme),
    DetailField("PATH", path, theme),
    DetailField("LAST ACTIVATED", activated_name, theme),
    Spacer(),
    Column {
      Text("Interaction")
          .Style({Font::System(13.0F).WithWeight(FontWeight::SemiBold), theme.colors.on_surface}),
      Text("Use the disclosure arrow for a single-click toggle. On touch screens, tapping a branch also expands it.")
          .Style({Font::System(12.0F), theme.colors.on_surface_variant}),
      Text("Press Enter to activate the focused row.")
          .Style({Font::System(12.0F), theme.colors.on_surface_variant}),
    }.With(
        Padding(14.0F),
        Spacing(6.0F),
        Background(WithOpacity(theme.colors.primary, 0.07F)),
        CornerRadius(12.0F),
        CrossAlign(CrossAxisAlignment::Stretch)),
  }.With(
      Frame{.min_width = 280.0F, .max_width = 340.0F, .min_height = 514.0F},
      Padding(20.0F),
      Spacing(16.0F),
      Background(theme.colors.surface),
      Border{WithOpacity(theme.colors.outline, 0.20F), 1.0F},
      CornerRadius(18.0F),
      Shadow{
          .color = WithOpacity(theme.colors.scrim, 0.10F),
          .offset = {0.0F, 6.0F},
          .blur_radius = 22.0F,
          .spread = -7.0F,
      },
      CrossAlign(CrossAxisAlignment::Stretch));
}

[[huxerui::composable]]
View FileTreeContent() {
  auto tasks = UseTaskScope();
  auto model = UseState(CreateFileTree());
  const ThemeSpec& theme = UseTheme();
  const TreeViewStyle tree_style{
      .background = Color::Transparent(),
      .foreground = theme.colors.on_surface,
      .disabled_foreground = WithOpacity(theme.colors.on_surface, theme.interactions.disabled_opacity),
      .selected_background = WithOpacity(theme.colors.primary, 0.13F),
      .active_background = WithOpacity(theme.colors.primary, 0.08F),
      .focus_indicator = theme.colors.primary,
      .item_extent = 38.0F,
      .indentation = 18.0F,
      .indicator_size = 12.0F,
      .item_padding = 8.0F,
      .indication = theme.interactions.indication,
  };

  auto tree = TreeView<Node>(model->roots, [](const Node& node) {
    return FileRow(node, node->children_state).Key(node->name);
  }, [model](const Node& node) {
    return TreeItemInfo{
        .label = node->children_state == ChildrenState::Loading ? node->name + ", loading" : node->name,
        .expandable = node->directory,
        .expanded = node->expanded,
        .selected = model->selected == node,
    };
  }).Children([](const Node& node) {
    return node->children;
  }).OnExpandedChanged([=](const Node& borrowed_node, bool expanded) {
    const Node node = borrowed_node;
    const bool load_children = expanded && node->children_state == ChildrenState::Deferred;
    model.Update([&](FileTreeModel&) {
      node->expanded = expanded;
      if (load_children) {
        node->children_state = ChildrenState::Loading;
      }
    });
    if (load_children) {
      tasks.Launch([model, node]() -> Task<void> {
        co_await Delay(1s);
        model.Update([&](FileTreeModel&) {
          node->children = LoadedExamples();
          node->children_state = ChildrenState::Loaded;
        });
      });
    }
  }).OnSelectionChanged([model](const Node& node, bool selected) {
    model.Update([&](FileTreeModel& value) {
      if (selected) {
        value.selected = node;
      } else if (value.selected == node) {
        value.selected.reset();
      }
    });
  }).OnActivated([model](const Node& node) {
    model.Update([&](FileTreeModel& value) { value.activated = node; });
  }).Label("Project files")
      .DisclosureIcon(tree_view::images::disclosure)
      .CacheExtent(152.0F)
      .Style(tree_style)
      .With(Frame{.height = 390.0F});

  return ScrollView {
    Column {
      Column {
        Flow {
          Column {
            Text("TREEVIEW EXAMPLE", TextRole::Label)
                .Style({Font::System(11.0F).WithWeight(FontWeight::SemiBold), theme.colors.primary}),
            Text("Project explorer")
                .Style({Font::System(32.0F).WithWeight(FontWeight::Bold), theme.colors.on_surface}),
            Text("Controlled hierarchy, virtual rows, and asynchronous children in one focused example.")
                .Style({Font::System(14.0F), theme.colors.on_surface_variant}),
          }.With(Spacing(6.0F)),
          Row {
            Badge("VIRTUALIZED", theme.colors.on_primary_container, theme.colors.primary_container),
            Badge("ASYNC CHILDREN", theme.colors.on_surface_variant, theme.colors.surface_container_high),
          }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Center)),
        }.With(Spacing(theme.spacing.large), CrossAlign(CrossAxisAlignment::Center)),
        Flow {
          ProjectTreeCard(std::move(tree), CountNodes(model->roots), IsLoading(model->roots), theme),
          InspectorCard(model->selected, model->activated, theme),
        }.With(Spacing(theme.spacing.large), CrossAlign(CrossAxisAlignment::Start)),
      }.With(
          Frame{.max_width = 940.0F},
          Spacing(theme.spacing.large),
          CrossAlign(CrossAxisAlignment::Stretch)
      ),
    }.With(
        Padding(EdgeInsets::Symmetric(theme.spacing.medium, 40.0F)),
        CrossAlign(CrossAxisAlignment::Center)
    ),
  }.With(ScrollBar(), Background(theme.colors.background));
}

View App() {
  return MaterialTheme {FileTreeContent()};
}

const Application application{
    App,
    {
        .window = {
            .title = "HuxerUI Project Explorer",
            .initial_size = {1040.0F, 720.0F},
        },
    }
};
