/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2013 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "nonEqPattersonJumpTFvPatchScalarField.H"
#include "addToRunTimeSelectionTable.H"
#include "fvPatchFieldMapper.H"
#include "volFields.H"
#include "mathematicalConstants.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::nonEqPattersonJumpTFvPatchScalarField::nonEqPattersonJumpTFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF
)
:
    mixedFvPatchScalarField(p, iF),
    UName_("U"),
    rhoName_("rho"),
    TtName_("Tt"),
    muName_("mu"),
    alphaName_("alphatr"), // NEW VINCENT 03/03/2016
    gammatrName_("gammatr"), // NEW VINCENT 03/03/2016
    mfpName_("mfp"), // NEW VINCENT 28/02/2016
    accommodationCoeff_(1.0),
    Twall_(p.size(), 0.0)/*,
    gamma_(1.4)*/
{
    refValue() = 0.0;
    refGrad() = 0.0;
    valueFraction() = 0.0;
}


Foam::nonEqPattersonJumpTFvPatchScalarField::nonEqPattersonJumpTFvPatchScalarField
(
    const nonEqPattersonJumpTFvPatchScalarField& ptf,
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF,
    const fvPatchFieldMapper& mapper
)
:
    mixedFvPatchScalarField(ptf, p, iF, mapper),
    UName_(ptf.UName_),
    rhoName_(ptf.rhoName_),
    TtName_(ptf.TtName_),
    muName_(ptf.muName_),
    alphaName_(ptf.alphaName_), // NEW VINCENT 03/03/2016
    gammatrName_(ptf.gammatrName_), // NEW VINCENT 03/03/2016
    mfpName_(ptf.mfpName_), // NEW VINCENT 28/02/2016
    accommodationCoeff_(ptf.accommodationCoeff_),
    Twall_(ptf.Twall_)/*,
    gamma_(ptf.gamma_)*/
{}


Foam::nonEqPattersonJumpTFvPatchScalarField::nonEqPattersonJumpTFvPatchScalarField
(
    const fvPatch& p,
    const DimensionedField<scalar, volMesh>& iF,
    const dictionary& dict
)
:
    mixedFvPatchScalarField(p, iF),
    UName_(dict.lookupOrDefault<word>("U", "U")),
    rhoName_(dict.lookupOrDefault<word>("rho", "rho")),
    TtName_(dict.lookupOrDefault<word>("Tt", "Tt")),
    muName_(dict.lookupOrDefault<word>("mu", "mu")),
    alphaName_(dict.lookupOrDefault<word>("alphatr", "alphatr")), // NEW VINCENT 03/03/2016
    gammatrName_(dict.lookupOrDefault<word>("gammatr", "gammatr")), // NEW VINCENT 03/03/2016
    mfpName_(dict.lookupOrDefault<word>("mfp", "mfp")), // NEW VINCENT 28/02/2016
    accommodationCoeff_(readScalar(dict.lookup("accommodationCoeff"))),
    Twall_("Twall", dict, p.size())/*,
    gamma_(dict.lookupOrDefault<scalar>("gamma", 1.4))*/
{
    if
    (
        mag(accommodationCoeff_) < SMALL
     || mag(accommodationCoeff_) > 2.0
    )
    {
        FatalIOErrorIn
        (
            "nonEqPattersonJumpTFvPatchScalarField::"
            "nonEqPattersonJumpTFvPatchScalarField"
            "("
            "    const fvPatch&,"
            "    const DimensionedField<scalar, volMesh>&,"
            "    const dictionary&"
            ")",
            dict
        )   << "unphysical accommodationCoeff specified"
            << "(0 < accommodationCoeff <= 1)" << endl
            << exit(FatalIOError);
    }

    if (dict.found("value"))
    {
        fvPatchField<scalar>::operator=
        (
            scalarField("value", dict, p.size())
        );
    }
    else
    {
        fvPatchField<scalar>::operator=(patchInternalField());
    }

    refValue() = *this;
    refGrad() = 0.0;
    valueFraction() = 0.0;
}


Foam::nonEqPattersonJumpTFvPatchScalarField::nonEqPattersonJumpTFvPatchScalarField
(
    const nonEqPattersonJumpTFvPatchScalarField& ptpsf,
    const DimensionedField<scalar, volMesh>& iF
)
:
    mixedFvPatchScalarField(ptpsf, iF),
    accommodationCoeff_(ptpsf.accommodationCoeff_),
    Twall_(ptpsf.Twall_)/*,
    gamma_(ptpsf.gamma_)*/
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

// Map from self
void Foam::nonEqPattersonJumpTFvPatchScalarField::autoMap
(
    const fvPatchFieldMapper& m
)
{
    mixedFvPatchScalarField::autoMap(m);
}


// Reverse-map the given fvPatchField onto this fvPatchField
void Foam::nonEqPattersonJumpTFvPatchScalarField::rmap
(
    const fvPatchField<scalar>& ptf,
    const labelList& addr
)
{
    mixedFvPatchField<scalar>::rmap(ptf, addr);
}


// Update the coefficients associated with the patch field
void Foam::nonEqPattersonJumpTFvPatchScalarField::updateCoeffs()
{
    if (updated())
    {
        return;
    }

    const fvPatchScalarField& pmu =
        patch().lookupPatchField<volScalarField, scalar>(muName_);
    const fvPatchScalarField& pTt =
        patch().lookupPatchField<volScalarField, scalar>(TtName_);    
    const fvPatchScalarField& palpha =
        patch().lookupPatchField<volScalarField, scalar>(alphaName_); // NEW VINCENT 03/03/2016
    const fvPatchScalarField& pgammatr =
        patch().lookupPatchField<volScalarField, scalar>(gammatrName_); // NEW VINCENT 03/03/2016     
    const fvPatchScalarField& pmfp =
        patch().lookupPatchField<volScalarField, scalar>(mfpName_); // NEW VINCENT 28/02/2016           
    const fvPatchScalarField& prho =
        patch().lookupPatchField<volScalarField, scalar>(rhoName_);
    const fvPatchVectorField& pU =
        patch().lookupPatchField<volVectorField, vector>(UName_);

    Field<scalar> C2
    (
        pmfp*2.0/(pgammatr + 1.0)/(pmu/palpha) // Pr = mu*Cp/k = mu/alpha * Cp/Cv = mu/alpha * gamma
        *(2.0 - accommodationCoeff_)/accommodationCoeff_ *Twall_/pTt
    ); // NEW VINCENT 03/03/2016

    Field<scalar> aCoeff(prho.snGrad() - prho/C2);
    Field<scalar> KEbyRho(0.5*magSqr(pU));

    valueFraction() = (1.0/(1.0 + patch().deltaCoeffs()*C2));
    refValue() = Twall_;
    refGrad() = 0.0;

    mixedFvPatchScalarField::updateCoeffs();
}


// Write
void Foam::nonEqPattersonJumpTFvPatchScalarField::write(Ostream& os) const
{
    fvPatchScalarField::write(os);

    writeEntryIfDifferent<word>(os, "U", "U", UName_);
    writeEntryIfDifferent<word>(os, "rho", "rho", rhoName_);
    //writeEntryIfDifferent<word>(os, "psi", "thermo:psi", psiName_);
    writeEntryIfDifferent<word>(os, "mu", "mu", muName_);
    writeEntryIfDifferent<word>(os, "alphatr", "alphatr", alphaName_); // NEW VINCENT 03/03/2016
    writeEntryIfDifferent<word>(os, "gammatr", "gammatr", gammatrName_); // NEW VINCENT 03/03/2016
    writeEntryIfDifferent<word>(os, "mfp", "mfp", mfpName_); // NEW VINCENT 28/02/2016

    os.writeKeyword("accommodationCoeff")
        << accommodationCoeff_ << token::END_STATEMENT << nl;
    Twall_.writeEntry("Twall", os);
    /*os.writeKeyword("gamma")
        << gamma_ << token::END_STATEMENT << nl;*/
    writeEntry("value", os);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    makePatchTypeField
    (
        fvPatchScalarField,
        nonEqPattersonJumpTFvPatchScalarField
    );
}


// ************************************************************************* //
