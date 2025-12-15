import numpy as np
from tensorflow import keras
import joblib
import os

os.chdir(r'C:\Users\valen\OneDrive\Documents\Projet_IOT')

print(f"Dossier actuel: {os.getcwd()}\n")

# Vérifier que les fichiers existent
fichiers_requis = [
    'epilepsy_model.h5',
    'scaler.pkl',
    'epilepsy_data_prepared.npz'
]

print("Fichiers dans le dossier:")
for f in os.listdir('.'):
    if f.endswith(('.h5', '.pkl', '.npz', '.cpp', '.docx', '.md', '.png')):
        print(f"  {f}")
print()

manquants = []
for fichier in fichiers_requis:
    if os.path.exists(fichier):
        print(f"{fichier} trouvé")
    else:
        print(f"{fichier} MANQUANT")
        manquants.append(fichier)

if manquants:
    print(f"\n ERREUR: {len(manquants)} fichier(s) manquant(s)!")
    exit(1)

print("\n" + "="*70)
print("CHARGEMENT DU MODÈLE")
print("="*70)

model = keras.models.load_model('epilepsy_model.h5')
print("Modèle chargé")

scaler = joblib.load('scaler.pkl')
print("Scaler chargé")

data = np.load('epilepsy_data_prepared.npz')
print("Données chargées")

print("\n" + "="*70)
print("TEST DU MODÈLE")
print("="*70)

X_test = data['X_test']
y_test = data['y_test']

print(f"\nNombre d'échantillons de test: {len(X_test)}")
print("Prédiction en cours...")

predictions_proba = model.predict(X_test, verbose=0)
predictions = (predictions_proba > 0.5).astype(int).flatten()

accuracy = (predictions == y_test).mean() * 100

print("\n" + "="*70)
print("RÉSULTATS")
print("="*70)

print(f"\nAccuracy: {accuracy:.2f}%")

vrais_positifs = np.sum((predictions == 1) & (y_test == 1))
faux_positifs = np.sum((predictions == 1) & (y_test == 0))
faux_negatifs = np.sum((predictions == 0) & (y_test == 1))
vrais_negatifs = np.sum((predictions == 0) & (y_test == 0))

print(f"\nDétection:")
print(f"  • Crises détectées correctement: {vrais_positifs}/{np.sum(y_test == 1)}")
print(f"  • Normal détecté correctement: {vrais_negatifs}/{np.sum(y_test == 0)}")
print(f"  • Fausses alarmes: {faux_positifs}")
print(f"  • Crises manquées: {faux_negatifs}")

sensitivity = vrais_positifs / (vrais_positifs + faux_negatifs) * 100
specificity = vrais_negatifs / (vrais_negatifs + faux_positifs) * 100

print(f"\nMétriques:")
print(f"  • Sensitivity (détection crises): {sensitivity:.2f}%")
print(f"  • Specificity (éviter fausses alarmes): {specificity:.2f}%")

print("\n" + "="*70)
print("test terminé !")
print("="*70)