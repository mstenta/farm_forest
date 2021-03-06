<?php

/**
 * @file
 * Farm forest plan planting form.
 */

/**
 * Forest plan planting form.
 */
function farm_forest_plan_planting_form($form, &$form_state, $plan) {

  // Start an empty form array.
  $form = array();

  // Set the page title.
  drupal_set_title('Planting');

  // Load area(s) associated with this plan.
  $area_ids = farm_plan_linked_records('area', $plan->id);

  // If the plan does not have any areas, show a message and bail.
  if (empty($area_ids)) {
    drupal_set_message(t('Areas must be assigned to this plan before a planting can be created.'), 'warning');
    return $form;
  }

  // Treat the form as a tree (no pun intended).
  $form['#tree'] = TRUE;

  // Add the plan to the form values, for use in form submission.
  $form['plan'] = array(
    '#type' => 'value',
    '#value' => $plan,
  );

  // Load asset(s) associated with this plan.
  $asset_ids = farm_plan_linked_records('asset', $plan->id);
  $assets = farm_asset_load_multiple($asset_ids);

  // Iterate through the assets to make a list of plantings.
  $plantings = array();
  foreach ($assets as $asset) {

    // If the asset is not a planting, skip it.
    if ($asset->type != 'planting') {
      continue;
    }

    // Add it to the list.
    $plantings[] = $asset;
  }

  // Iterate through the plantings and build forms.
  foreach ($plantings as $planting) {

    // Add a planting details form.
    $form['plantings'][$planting->id] = farm_forest_plan_planting_details_form($plan, $planting);

    // Make it a fieldset with a title.
    $form['plantings'][$planting->id]['#type'] = 'fieldset';
    $form['plantings'][$planting->id]['#title'] = t('Planting') . ': ' . entity_label('farm_asset', $planting);

    // Add the planting to the form values, for use in form submission.
    $form['plantings'][$planting->id]['planting'] = array(
      '#type' => 'value',
      '#value' => $planting,
    );

    // If the planting has an ID, add a link to remove it.
    if (!empty($planting->id)) {
      $form['plantings'][$planting->id]['remove'] = array(
        '#type' => 'markup',
        '#markup' => l(t('Remove'), 'farm/plan/' . $plan->id . '/asset/' . $planting->id . '/delete', array('query' => array('destination' => current_path()))),
      );
    }
  }

  // Add a blank form for creating a new planting.
  $form['new'] = farm_forest_plan_planting_details_form($plan);
  $form['new']['#type'] = 'fieldset';
  $form['new']['#title'] = t('Create a planting');

  // If there are plantings already, collapse the new planting form.
  if (!empty($plantings)) {
    $form['new']['#collapsible'] = TRUE;
    $form['new']['#collapsed'] = TRUE;
  }

  // Submit button for saving all changes.
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 100,
  );

  // Return the form.
  return $form;
}

/**
 * Planting details sub-form.
 *
 * @param $plan
 *   The plan entity.
 * @param $planting
 *   The planting asset, or NULL.
 *
 * @return array
 *   Returns a form array that can be used in another form.
 *
 * @see farm_forest_plan_planting_form().
 */
function farm_forest_plan_planting_details_form($plan, $planting=NULL) {

  // If a plan ID is not available, bail.
  if (empty($plan->id)) {
    return array();
  }

  // Define the date format.
  $date_format = 'Y-m-d';

  // Define default values.
  $defaults = array(
    'species' => '',
    'date' => REQUEST_TIME,
    'areas' => array(),
    'count' => '',
    'spacing' => '',
  );

  // Load areas that are associated with the plan and build a list of form
  // select options.
  $area_ids = farm_plan_linked_records('area', $plan->id);
  $areas = taxonomy_term_load_multiple($area_ids);
  $area_options = array();
  foreach ($areas as $area) {
    $area_options[$area->tid] = entity_label('taxonomy_term', $area);
  }

  // If a planting is provided, load default values.
  $transplanting_log = NULL;
  if (!empty($planting)) {

    // Get the planting metadata wrapper.
    $planting_wrapper = entity_metadata_wrapper('farm_asset', $planting);

    // Get the species name.
    $defaults['species'] = $planting_wrapper->field_farm_crop->label();

    // Find transplanting logs that reference the planting.
    $query = farm_log_asset_query($planting->id, 0, NULL, 'farm_transplanting');
    $query->addField('ss_log', 'id');
    $result = $query->execute();
    foreach ($result as $row) {
      if (!empty($row->id)) {
        $transplanting_log = log_load($row->id);
      }
    }

    // If a log was found, get default values from it.
    if (!empty($transplanting_log)) {

      // Get the date.
      $defaults['date'] = $transplanting_log->timestamp;

      // Iterate through areas already being referenced by the log, add them to
      // the available area options, and build an array of default values.
      if (!empty($transplanting_log->field_farm_movement[LANGUAGE_NONE][0]['value'])) {
        $movement = field_collection_item_load($transplanting_log->field_farm_movement[LANGUAGE_NONE][0]['value']);
        if (!empty($movement->field_farm_move_to[LANGUAGE_NONE])) {
          foreach ($movement->field_farm_move_to[LANGUAGE_NONE] as $area_ref) {

            // If the term ID is empty, skip it.
            if (empty($area_ref['tid'])) {
              continue;
            }
            $area_id = $area_ref['tid'];

            // Load the area. If it can't be loaded, skip it.
            $area = taxonomy_term_load($area_id);
            if (empty($area)) {
              continue;
            }

            // If the area is not in $areas, add it.
            if (!array_key_exists($area_id, $areas)) {
              if (!empty($area)) {
                $areas[$area_id] = $area;
              }
            }

            // Add to the list of area options.
            $area_options[$area_id] = entity_label('taxonomy_term', $area);

            // Add to the list of selected areas.
            $defaults['areas'][] = $area_id;
          }
        }
      }

      // Get the count.
      $count_measure = farm_quantity_log_data($transplanting_log, 'count', t('total planted'));
      if (!empty($count_measure[0]['value'])) {
        $defaults['count'] = $count_measure[0]['value'];
      }

      // Get the spacing.
      $spacing_measure = farm_quantity_log_data($transplanting_log, 'length', t('tree spacing'));
      if (!empty($spacing_measure[0]['value'])) {
        $defaults['spacing'] = $spacing_measure[0]['value'];
      }
    }
  }

  // Tree species.
  $form['species'] = array(
    '#type' => 'textfield',
    '#title' => t('Tree species'),
    '#autocomplete_path' => 'taxonomy/autocomplete/field_farm_crop',
    '#default_value' => $defaults['species'],
  );

  // Transplanting log.
  $form['transplanting']['log'] = array(
    '#type' => 'value',
    '#value' => $transplanting_log,
  );

  // Transplanting date.
  $form['transplanting']['date'] = array(
    '#type' => 'date_select',
    '#title' => t('Planting date'),
    '#date_format' => $date_format,
    '#date_label_position' => 'within',
    '#date_year_range' => '-3:+10',
    '#default_value' => date($date_format, $defaults['date']),
  );

  // If no selected assets were found above, and this is not an existing log,
  // select all by default.
  if (empty($selected_areas) && empty($transplanting_log)) {
    $defaults['areas'] = array_keys($area_options);
  }

  // Transplanting areas.
  $form['transplanting']['areas'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Areas'),
    '#options' => $area_options,
    '#default_value' => $defaults['areas'],
  );

  // Total trees planted.
  $form['transplanting']['count'] = array(
    '#type' => 'textfield',
    '#title' => t('Total trees planted'),
    '#input_group' => TRUE,
    '#field_suffix' => t('trees'),
    '#element_validate' => array('element_validate_integer_positive'),
    '#default_value' => $defaults['count'],
  );

  // Load the system of measurement.
  $system_of_measurement = farm_quantity_system_of_measurement();
  $units = 'meters';
  if ($system_of_measurement == 'us') {
    $units = 'feet';
  }

  // Tree spacing.
  $form['transplanting']['spacing'] = array(
    '#type' => 'textfield',
    '#title' => t('Tree spacing'),
    '#input_group' => TRUE,
    '#field_suffix' => t($units),
    '#element_validate' => array('element_validate_number'),
    '#default_value' => $defaults['spacing'],
  );
  $form['transplanting']['spacing_units'] = array(
    '#type' => 'hidden',
    '#value' => $units,
  );

  // Return the form array.
  return $form;
}

/**
 * Forest plan planting form validation.
 */
function farm_forest_plan_planting_form_validate($form, &$form_state) {

}

/**
 * Forest plan planting form submit.
 */
function farm_forest_plan_planting_form_submit($form, &$form_state) {

  // Get the plan.
  $plan = $form_state['values']['plan'];

  // Create an array to add logs to.
  $logs = array();

  // If a new planting species and date are provided, create them.
  if (!empty($form_state['values']['new']['species']) && !empty($form_state['values']['new']['transplanting']['date'])) {

    // Create a name for the planting.
    $planting_name = t('@species planting (@plan)', array('@species' => $form_state['values']['new']['species'], '@plan' => entity_label('farm_plan', $plan)));

    // Create a new planting asset.
    $values = array(
      'type' => 'planting',
      'name' => $planting_name,
    );
    $planting_asset = entity_create('farm_asset', $values);
    $planting_wrapper = entity_metadata_wrapper('farm_asset', $planting_asset);

    // Get the species term (or create a new one), and add it to the planting.
    $species = farm_term($form_state['values']['new']['species'], 'farm_crops');
    $planting_wrapper->field_farm_crop[] = $species;

    // Save the planting.
    $planting_wrapper->save();

    // Link the planting to the plan.
    farm_plan_link_record('asset', $plan->id, $planting_asset->id);

    // Set a message.
    $label = entity_label('farm_asset', $planting_asset);
    $uri = entity_uri('farm_asset', $planting_asset);
    drupal_set_message('Planting created: ' . l($label, $uri['path']));

    // Load selected areas.
    $areas = array();
    if (!empty($form_state['values']['new']['transplanting']['areas'])) {
      foreach ($form_state['values']['new']['transplanting']['areas'] as $area_id) {
        if (empty($area_id)) {
          continue;
        }
        $area = taxonomy_term_load($area_id);
        if (!empty($area)) {
          $areas[] = $area;
        }
      }
    }

    // Convert the date to a timestamp.
    $timestamp = strtotime($form_state['values']['new']['transplanting']['date']);

    // If the timestamp is in the past, the log is done. Otherwise, it is not.
    $done = FALSE;
    if ($timestamp < REQUEST_TIME) {
      $done = TRUE;
    }

    // Create a new transplanting log, using the movement log helper function.
    $log = farm_movement_create($planting_asset, $areas, $timestamp, 'farm_transplanting', $done);

    // Add the log to the array.
    $logs[] = $log;

    // Link the log to the plan.
    farm_plan_link_record('log', $plan->id, $log->id);

    // Add quantity measurements, if available.
    $measurements = array();
    if (!empty($form_state['values']['new']['transplanting']['count'])) {
      $measurements[] = array(
        'measure' => 'count',
        'value' => $form_state['values']['new']['transplanting']['count'],
        'units' => t('trees'),
        'label' => t('total planted'),
      );
    }
    if (!empty($form_state['values']['new']['transplanting']['spacing'])) {
      $measurements[] = array(
        'measure' => 'length',
        'value' => $form_state['values']['new']['transplanting']['spacing'],
        'units' => t($form_state['values']['new']['transplanting']['spacing_units']),
        'label' => t('tree spacing'),
      );
    }

    // If there are quantity measurements, add them to the log.
    if (!empty($measurements)) {
      farm_quantity_log_add_measurements($log, $measurements);
    }
  }

  // Iterate through existing plantings and update as necessary.
  if (!empty($form_state['values']['plantings'])) {
    foreach ($form_state['values']['plantings'] as $planting_id => $planting_values) {

      // Keep track of what was updated, so we can set a message afterwards.
      $updated_planting = FALSE;
      $updated_transplanting = FALSE;

      // If the species changed, update the planting.
      if ($planting_values['species'] != $form['plantings'][$planting_id]['species']['#default_value']) {

        // Get an entity metadata wrappers for the planting.
        $planting_wrapper = entity_metadata_wrapper('farm_asset', $planting_values['planting']);

        // Get the species term (or create a new one), and add it to the planting.
        $species = farm_term($planting_values['species'], 'farm_crops');
        $planting_wrapper->field_farm_crop->set(array(NULL));
        $planting_wrapper->field_farm_crop[] = $species;

        // Save the planting.
        $planting_wrapper->save();

        // Remember that the planting was updated.
        $updated_planting = TRUE;
      }

      // If the planting date changed, update the log.
      if ($planting_values['transplanting']['date'] != $form['plantings'][$planting_id]['transplanting']['date']['#default_value']) {

        // Update the log timestamp.
        $timestamp = strtotime($planting_values['transplanting']['date']);
        $planting_values['transplanting']['log']->timestamp = $timestamp;
        log_save($planting_values['transplanting']['log']);

        // Remember that the transplanting was updated.
        $updated_transplanting = TRUE;
      }

      // Sort new selected areas and default selected areas for comparison.
      $selected_areas = array_values(array_filter($planting_values['transplanting']['areas']));
      $selected_areas = array_values(array_filter($planting_values['transplanting']['areas']));
      $default_areas = array_values($form['plantings'][$planting_id]['transplanting']['areas']['#default_value']);
      sort($selected_areas);
      sort($default_areas);

      // If the areas changed, update the log.
      if ($selected_areas != $default_areas) {

        // Load the movement information associated with the log.
        if (!empty($planting_values['transplanting']['log']->field_farm_movement[LANGUAGE_NONE][0]['value'])) {
          $movement = field_collection_item_load($planting_values['transplanting']['log']->field_farm_movement[LANGUAGE_NONE][0]['value']);
          $movement_wrapper = entity_metadata_wrapper('field_collection_item', $movement);
          $movement_wrapper->field_farm_move_to = array();
          foreach ($selected_areas as $area_id) {
            $area = taxonomy_term_load($area_id);
            $movement_wrapper->field_farm_move_to[] = $area;
          }
          $movement_wrapper->save();
        }

        // Remember that the transplanting was updated.
        $updated_transplanting = TRUE;
      }

      // If the count changed, update the log.
      if ($planting_values['transplanting']['count'] != $form['plantings'][$planting_id]['transplanting']['count']['#default_value']) {

        // Update the 'total planted' quantity measurement.
        $log = $planting_values['transplanting']['log'];
        $value = $planting_values['transplanting']['count'];
        farm_quantity_log_update_measurement_by_label($log, 'total planted', $value);

        // Add the log to the array.
        $logs[] = $log;

        // Remember that the transplanting was updated.
        $updated_transplanting = TRUE;
      }

      // If the spacing changed, update the log.
      if ($planting_values['transplanting']['spacing'] != $form['plantings'][$planting_id]['transplanting']['spacing']['#default_value']) {

        // Update the 'tree spacing' quantity measurement.
        $log = $planting_values['transplanting']['log'];
        $value = $planting_values['transplanting']['spacing'];
        farm_quantity_log_update_measurement_by_label($log, 'tree spacing', $value);

        // Add the log to the array.
        $logs[] = $log;

        // Remember that the transplanting was updated.
        $updated_transplanting = TRUE;
      }

      // If the planting was updated, display a message.
      if ($updated_planting) {
        $planting_name = entity_label('farm_asset', $planting_values['planting']);
        drupal_set_message(t('The planting asset %planting_name was updated.', array('%planting_name' => $planting_name)));
      }

      // If the transplanting was updated, display a message.
      if ($updated_transplanting) {
        $transplanting_name = entity_label('log', $planting_values['transplanting']['log']);
        drupal_set_message(t('The transplanting log %transplanting_name was updated.', array('%transplanting_name' => $transplanting_name)));
      }
    }
  }

  // Add logs to the form state storage so future submit handlers can use them.
  $form_state['storage']['logs'] = $logs;
}
