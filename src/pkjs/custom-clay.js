// Page de reglages : n'affiche que les champs utiles et convertit les
// valeurs quand on change d'unites. La montre recoit les valeurs de l'unite
// choisie et les convertit en metrique.
module.exports = function () {
  var clayConfig = this;
  var CM_PER_INCH = 2.54;
  var KG_PER_LB = 0.45359237;
  var KM_PER_MILE = 1.609344;

  var numericLanguage = (navigator.language || 'en').substring(0, 2);
  var numericErrors = {
    en: 'Enter a whole number from {min} to {max}, in increments of {step}.',
    fr: 'Saisissez un entier de {min} a {max}, par pas de {step}.',
    de: 'Geben Sie eine ganze Zahl von {min} bis {max} in Schritten von {step} ein.',
    es: 'Introduzca un entero de {min} a {max}, en pasos de {step}.',
    it: 'Inserisci un intero da {min} a {max}, con incrementi di {step}.',
    pt: 'Introduza um inteiro de {min} a {max}, em passos de {step}.'
  };

  function numericFields() {
    return document.querySelectorAll('input[type="number"]');
  }

  function syncNumericFields() {
    var fields = numericFields();
    for (var i = 0; i < fields.length; i++) {
      var field = fields[i];
      // Un champ masque ne doit pas bloquer la validation HTML.
      field.disabled = field.getClientRects().length === 0;
      field.setCustomValidity('');
    }
  }

  function validateNumericFields(event) {
    syncNumericFields();
    var fields = numericFields();

    for (var i = 0; i < fields.length; i++) {
      var field = fields[i];
      if (field.disabled) {
        continue;
      }

      var raw = String(field.value).trim();
      var value = Number(raw);
      var minimum = Number(field.min);
      var maximum = Number(field.max);
      var step = Number(field.step) || 1;
      var valid = /^\d+$/.test(raw) &&
          isFinite(value) &&
          value >= minimum && value <= maximum &&
          (value - minimum) % step === 0;

      if (!valid) {
        var message = (numericErrors[numericLanguage] || numericErrors.en)
            .replace('{min}', field.min)
            .replace('{max}', field.max)
            .replace('{step}', field.step);

        field.setCustomValidity(message);
        event.preventDefault();
        event.stopImmediatePropagation();
        field.focus();

        if (typeof field.reportValidity === 'function') {
          field.reportValidity();
        } else {
          window.alert(message);
        }
        return false;
      }
    }
    return true;
  }

  function installNumericValidation() {
    // Capture avant les gestionnaires d'enregistrement de Clay.
    document.addEventListener('click', function (event) {
      var target = event.target;
      while (target && target !== document) {
        var tag = target.tagName;
        var type = String(target.type || '').toLowerCase();
        if (tag === 'BUTTON' ||
            (tag === 'INPUT' && (type === 'submit' || type === 'button'))) {
          validateNumericFields(event);
          return;
        }
        target = target.parentNode;
      }
    }, true);

    // Couvre aussi une soumission au clavier.
    document.addEventListener('submit', validateNumericFields, true);

    document.addEventListener('input', function (event) {
      var target = event.target;
      if (target && target.tagName === 'INPUT' && target.type === 'number') {
        target.setCustomValidity('');
      }
    }, true);
  }


  function item(key) {
    return clayConfig.getItemByMessageKey(key);
  }

  function setVisible(keys, visible) {
    keys.forEach(function (key) {
      if (visible) {
        item(key).show();
      } else {
        item(key).hide();
      }
    });
  }

  function isImperial() {
    return String(item('Units').get()) === '1';
  }

  function refresh() {
    var imperial = isImperial();
    var walkOn = !!item('StrideWalkOn').get();
    var runOn = !!item('StrideRunOn').get();

    setVisible(['Height', 'Weight'], !imperial);
    setVisible(['HeightFt', 'HeightIn', 'WeightLb'], imperial);
    setVisible(['StrideWalk'], walkOn && !imperial);
    setVisible(['StrideWalkInch'], walkOn && imperial);
    setVisible(['StrideRun'], runOn && !imperial);
    setVisible(['StrideRunInch'], runOn && imperial);
    setVisible(['ShowAmPm'], !!item('Clock12h').get());
    setVisible(['HideHourZero'], !item('Clock12h').get());
    setVisible(['RestingHr'], !!item('RestingHrOn').get());
    setVisible(['VibrateInterval', 'SoundInterval'],
               String(item('IntervalMin').get()) !== '0');
    syncNumericFields();
  }

  function toImperial() {
    var totalInches = Math.round(item('Height').get() / CM_PER_INCH);
    item('HeightFt').set(Math.floor(totalInches / 12));
    item('HeightIn').set(totalInches % 12);
    item('WeightLb').set(Math.round(item('Weight').get() / KG_PER_LB));
    item('StrideWalkInch').set(Math.round(item('StrideWalk').get() / CM_PER_INCH));
    item('StrideRunInch').set(Math.round(item('StrideRun').get() / CM_PER_INCH));
  }

  function toMetric() {
    var totalInches = Number(item('HeightFt').get()) * 12 + Number(item('HeightIn').get());
    item('Height').set(Math.round(totalInches * CM_PER_INCH));
    item('Weight').set(Math.round(item('WeightLb').get() * KG_PER_LB));
    item('StrideWalk').set(Math.round(item('StrideWalkInch').get() * CM_PER_INCH));
    item('StrideRun').set(Math.round(item('StrideRunInch').get() * CM_PER_INCH));
  }

  function milesToKilometers(miles) {
    return Math.round(Number(miles) * KM_PER_MILE);
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var lastImperial = isImperial();
    refresh();
    installNumericValidation();

    item('Units').on('change', function () {
      var imperial = isImperial();
      if (imperial !== lastImperial) {
        if (imperial) {
          toImperial();
        } else {
          toMetric();
        }
        lastImperial = imperial;
      }
      refresh();
    });

    item('StrideWalkOn').on('change', refresh);
    item('StrideRunOn').on('change', refresh);
    item('Clock12h').on('change', refresh);
    item('RestingHrOn').on('change', refresh);
    item('IntervalMin').on('change', refresh);
  });
};
