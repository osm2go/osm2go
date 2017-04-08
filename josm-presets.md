## OSM2go Preset System

This document is based on the [Vespucci](https://github.com/MarcusWolschon/osmeditor4android) preset documentation.

OSM2go uses JOSM compatible presets, although the support is only partial and not yet up to the current version of JOSM.

### Supported JOSM Preset Elements and Attributes

Note: this is loosely based on what [JOSM claims](https://josm.openstreetmap.de/wiki/TaggingPresets) to work like, this may, and actually likely is, different from the actual implementation. Language specific attributes are ignored. "Supported" doesn't necessarily imply the same behaviour as JOSM, simply that OSM2go will do something useful with the value.


Element            | Attributes                    | Support   | Notes
-------------------|-------------------------------|-----------|----------------------------------------------------------------
__&lt;presets&gt;__          |                               | supported | required
__&lt;!-- comment --&gt;__   |                               | ignored   |
__&lt;group&gt;__            |                               | supported |
__&nbsp;__                   | name                          | supported | required
__&nbsp;__                   | name_context                  | supported | ignored
__&nbsp;__                   | icon                          | supported | supported
__&lt;item&gt;__             |                               | supported |
__&nbsp;__                   | name                          | supported | required
__&nbsp;__                   | name_context                  | ignored   |
__&nbsp;__                   | icon                          | supported |
__&nbsp;__                   | type                          | supported |
__&nbsp;__                   | name_template                 | ignored   |
__&nbsp;__                   | preset_name_label             | supported |
__&lt;chunk&gt;__            |                               | supported |
__&nbsp;__                   | id                            | supported | required
__&lt;reference&gt;__        |                               | supported |
__&nbsp;__                   | ref                           | supported | required
__&lt;key&gt;__              |                               | supported |
__&nbsp;__                   | value                         | supported | required
__&nbsp;__                   | match                         | supported |
__&lt;text&gt;__             |                               | supported |
__&nbsp;__                   | key                           | supported | required
__&nbsp;__                   | text                          | supported |
__&nbsp;__                   | match                         | supported | "keyvalue" and "keyvalue!" behave like "key" and "key!"
__&nbsp;__                   | default                       | supported |
__&nbsp;__                   | use_last_as_default           | ignored   |
__&nbsp;__                   | auto_increment                | ignored   |
__&nbsp;__                   | length                        | ignored   |
__&nbsp;__                   | alternative_autocomplete_keys | ignored   |
__&lt;combo&gt;__            |                               | supported |
__&nbsp;__                   | key                           | supported | required
__&nbsp;__                   | text                          | supported |
__&nbsp;__                   | text_context                  | ignored   |
__&nbsp;__                   | values                        | supported |
__&nbsp;__                   | values_sort                   | ignored   |
__&nbsp;__                   | delimiter                     | supported |
__&nbsp;__                   | default                       | supported |
__&nbsp;__                   | match                         | supported |
__&nbsp;__                   | display_values                | supported |
__&nbsp;__                   | short_descriptions            | ignored   |
__&nbsp;__                   | values_context                | ignored   |
__&nbsp;__                   | editable                      | ignored   | always treated as "false"
__&nbsp;__                   | use_last_as_default           | ignored   |
__&nbsp;__                   | values_searchable             | ignored   |
__&nbsp;__                   | length                        | ignored   |
__&nbsp;__                   | values_no_i18n                | ignored   |
__&lt;multiselect&gt;__      |                               | ignored   |
__&nbsp;__                   | key                           | ignored   |
__&nbsp;__                   | text                          | ignored   |
__&nbsp;__                   | text_context                  | ignored   |
__&nbsp;__                   | values                        | ignored   |
__&nbsp;__                   | values_sort                   | ignored   |
__&nbsp;__                   | delimiter                     | ignored   |
__&nbsp;__                   | default                       | ignored   |
__&nbsp;__                   | match                         | ignored   |
__&nbsp;__                   | display_values                | ignored   |
__&nbsp;__                   | short_descriptions            | ignored   |
__&nbsp;__                   | values_context                | ignored   |
__&nbsp;__                   | editable                      | ignored   |
__&nbsp;__                   | use_last_as_default           | ignored   |
__&nbsp;__                   | values_searchable             | ignored   |
__&nbsp;__                   | length                        | ignored   |
__&nbsp;__                   | values_no_i18n                | ignored   |
__&nbsp;__                   | rows                          | ignored   |
__&lt;list_entry&gt;__       |                               | supported |
__&nbsp;__                   | value                         | supported | required
__&nbsp;__                   | display_value                 | supported |
__&nbsp;__                   | short_description             | ignored   |
__&nbsp;__                   | icon                          | ignored   |
__&nbsp;__                   | icon_size                     | ignored   |
__&lt;checkgroup&gt;__       |                               | ignored   | but not the included <check> elements
__&lt;check&gt;__            |                               | supported |
__&nbsp;__                   | key                           | supported | required
__&nbsp;__                   | text                          | supported |
__&nbsp;__                   | text_context                  | ignored   |
__&nbsp;__                   | value_on                      | supported |
__&nbsp;__                   | value_off                     | ignored   |
__&nbsp;__                   | disable_off                   | ignored   |
__&nbsp;__                   | default                       | supported | only checked for "on" or not
__&nbsp;__                   | match                         | supported |
__&lt;label&gt;__            |                               | supported |
__&nbsp;__                   | text                          | supported | required
__&lt;space/&gt;__           |                               | supported | ignored on Hildon
__&lt;optional&gt;__         |                               | ignored   | the contained items are parsed as if they were on the same level
__&nbsp;__                   | text                          | ignored   |
__&lt;separator/&gt;__       |                               | supported |
__&lt;item_separator/&gt;__  |                               | ignored   |
__&lt;link&gt;__             |                               | supported |
__&nbsp;__                   | href                          | supported |
__&lt;roles&gt;__            |                               | ignored   |
__&lt;role&gt;__             |                               | ignored   |
__&nbsp;__                   | key                           | ignored   | required
__&nbsp;__                   | text                          | ignored   |
__&nbsp;__                   | text_context                  | ignored   |
__&nbsp;__                   | requisite                     | ignored   |
__&nbsp;__                   | count                         | ignored   |
__&nbsp;__                   | type                          | ignored   |
__&nbsp;__                   | member_expression             | ignored   |
__&lt;preset_link&gt;__      |                               | supported |
__&nbsp;__                   | preset_name                   | supported | required
