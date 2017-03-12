## OSM2go Preset System

This document is based on the [Vespucci](https://github.com/MarcusWolschon/osmeditor4android) preset documentation.

OSM2go uses JOSM compatible presets, although the support is only partial and not yet up to the current version of JOSM.

### Supported JOSM Preset Elements and Attributes

Note: this is loosely based on what [JOSM claims](https://josm.openstreetmap.de/wiki/TaggingPresets) to work like, this may, and actually likely is, different from the actual implementation. Language specific attributes are ignored. "Supported" doesn't necessarily imply the same behaviour as JOSM, simply that OSM2go will do something useful with the value.


Element            | Attributes                     | Support   | Notes
-------------------|-------------------------------|-----------|----------------------------------------------------------------
__&lt;presets&gt;__          |                               | required   |
__&lt;!-- comment --&gt;__   |                               | ignored   |
__&lt;group&gt;__            |                               | supported |
                   | name                          | supported | required
                   | name_context                  | supported | ignored
                   | icon                          | supported | supported
__&lt;item&gt;__             |                               | supported |
                   | name                          | supported | required
                   | name_context                  | ignored   |
                   | icon                          | supported |
                   | type                          | supported |
                   | name_template                 | ignored   |
                   | preset_name_label             | supported |
__&lt;chunk&gt;__            |                               | supported | 
                   | id                            | supported | required
__&lt;reference&gt;__        |                               | supported |
                   | ref                           | supported | required
__&lt;key&gt;__              |                               | supported |
                   | value                         | supported | required
                   | match                         | ignored   | matches always work like "keyvalue"
__&lt;text&gt;__             |                               | supported |
                   | key                           | supported | required
                   | text                          | supported |
                   | match                         | ignored   | matches always work like "keyvalue"
                   | default                       | supported | 
                   | use_last_as_default           | ignored   | 
                   | auto_increment                | ignored   |
                   | length                        | ignored   |
                   | alternative_autocomplete_keys | ignored   |
__&lt;combo&gt;__            |                               | supported |
                   | key                           | supported | required
                   | text                          | supported |
                   | text_context                  | ignored   |
                   | values                        | supported |
                   | values_sort                   | ignored   |
                   | delimiter                     | supported |
                   | default                       | supported |
                   | match                         | ignored   | matches always work like "keyvalue"
                   | display_values                | ignored   |
                   | short_descriptions            | ignored   |
                   | values_context                | ignored   |
                   | editable                      | ignored   | always treated as "false"
                   | use_last_as_default           | ignored   |
                   | values_searchable             | ignored   |
                   | length                        | ignored   |
                   | values_no_i18n                | ignored   |
__&lt;multiselect&gt;__      |                               | ignored |
                   | key                           | ignored   |
                   | text                          | ignored   |
                   | text_context                  | ignored   |
                   | values                        | ignored   |
                   | values_sort                   | ignored   |
                   | delimiter                     | ignored   |
                   | default                       | ignored   |
                   | match                         | ignored   |
                   | display_values                | ignored   |
                   | short_descriptions            | ignored   |
                   | values_context                | ignored   |
                   | editable                      | ignored   |
                   | use_last_as_default           | ignored   |
                   | values_searchable             | ignored   |
                   | length                        | ignored   |
                   | values_no_i18n                | ignored   |
                   | rows                          | ignored   |
__&lt;list_entry&gt;__       |                               | ignored |   
                   | display_value                 | ignored   |
                   | short_description             | ignored   |
                   | icon                          | ignored   |
                   | icon_size                     | ignored   |
__&lt;checkgroup&gt;__       |                               | ignored   | but not the included <check> elements
__&lt;check&gt;__            |                               | supported |
                   | key                           | supported | required
                   | text                          | supported |
                   | text_context                  | ignored   |
                   | value_on                      | supported |
                   | value_off                     | ignored   | 
                   | disable_off                   | ignored   |
                   | default                       | supported | only checked for "on" or not
                   | match                         | ignored   | matches always work like "keyvalue"
__&lt;label&gt;__            |                               | supported |
                   | text                          | supported | required
__&lt;space/&gt;__          |                               | supported | ignored on Hildon
__&lt;optional&gt;__         |                               | ignored | the contained items are parsed as if they were on the same level
                   | text                          | ignored   |
__&lt;separator/&gt;__       |                               | supported |
__&lt;item_separator/&gt;__  |                               | ignored   |
__&lt;link&gt;__             |                               | supported |
                   | href                          | supported |
__&lt;roles&gt;__            |                               | ignored   |
__&lt;role&gt;__             |                               | ignored  |
                   | key                           | ignored   | required
                   | text                          | ignored   |
                   | text_context                  | ignored   | 
                   | requisite                     | ignored   |
                   | count                         | ignored   |
                   | type                          | ignored   |
                   | member_expression             | ignored   | 
