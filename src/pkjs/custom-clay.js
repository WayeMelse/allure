// Page de reglages : n'affiche que les champs utiles et convertit les
// valeurs quand on change d'unites. La montre recoit les valeurs de l'unite
// choisie et les convertit en metrique.
module.exports = function () {
  var clayConfig = this;
  var CM_PER_INCH = 2.54;
  var KG_PER_LB = 0.45359237;

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
    var totalInches = item('HeightFt').get() * 12 + item('HeightIn').get();
    item('Height').set(Math.round(totalInches * CM_PER_INCH));
    item('Weight').set(Math.round(item('WeightLb').get() * KG_PER_LB));
    item('StrideWalk').set(Math.round(item('StrideWalkInch').get() * CM_PER_INCH));
    item('StrideRun').set(Math.round(item('StrideRunInch').get() * CM_PER_INCH));
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var lastImperial = isImperial();
    refresh();

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
  });
};
