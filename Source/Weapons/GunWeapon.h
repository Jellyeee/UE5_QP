#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "TimerManager.h"
#include "GunWeapon.generated.h"

/**
 *
 */
UCLASS()
class PJ_QUIET_PROTOCOL_API AGunWeapon : public AWeaponBase
{
	GENERATED_BODY()

public:
	AGunWeapon();

	virtual void StartFire_Implementation() override;
	virtual void StopAttack_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Weapon|Gun|Ammo")
	FORCEINLINE int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Gun|Ammo")
	FORCEINLINE int32 GetMagCapacity() const { return MagCapacity; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Gun|Ammo")
	void AddAmmo(int32 AmountToAdd);

	void SpendRound();

	// Recoil Getters
	float GetRecoilPitchMin() const { return RecoilPitchMin; }
	float GetRecoilPitchMax() const { return RecoilPitchMax; }
	float GetRecoilYawMin() const { return RecoilYawMin; }
	float GetRecoilYawMax() const { return RecoilYawMax; }

protected:
	virtual void BeginPlay() override; // 샷건/권총 초기화 설정용
	
	void FireOnce();
	void FireSinglePellet(); // 단일 투사체 발사 기능 (샷건 순차 발사 지원)

	FTimerHandle ShotgunFireTimerHandle; // 샷건 발사 간격 타이머 핸들
	int32 PelletsFiredCount = 0; // 지금까지 발사된 샷건 총알 개수 추적용

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun", meta = (ClampMin = "0.0"))
	float Range = 15000.f; //사거리

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Weapon|Gun|Ammo")
	int32 CurrentAmmo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Gun|Ammo", meta = (ClampMin = "1"))
	int32 MagCapacity = 30; //최대 장탄수

	// Projectile
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Gun|Projectile")
	TSubclassOf<class AQPProjectileBullet> ProjectileBulletClass; //투사체 불릿 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Projectile")
	float BulletSpeed = 15000.f; //불릿 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Gun|Projectile")
	FName MuzzleSocketName = TEXT("MuzzleSocket"); //총구 소켓 이름
	
	// Shotgun Specific
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Shotgun", meta = (ClampMin = "1"))
	int32 ShotgunPelletCount = 8; // 샷건 발사 시 총알 개수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Shotgun", meta = (ClampMin = "0.0"))
	float ShotgunSpreadAngle = 5.f; // 샷건 탄착군 퍼짐 각도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Shotgun", meta = (ClampMin = "0.01"))
	float TimeBetweenPellets = 0.02f; // 샷건 총알 1발당 발사 간격

	// Handgun Specific
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Handgun", meta = (ClampMin = "0.0"))
	float HandgunRange = 5000.f; // 권총 사거리 (소총보다 짧게)

	// Recoil
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Recoil", meta = (ClampMin = "0.0"))
	float RecoilPitchMin = 0.5f; // 반동 위쪽 최소값

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Recoil", meta = (ClampMin = "0.0"))
	float RecoilPitchMax = 1.0f; // 반동 위쪽 최대값

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Recoil")
	float RecoilYawMin = -0.3f; // 반동 좌우 최소값

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Gun|Recoil")
	float RecoilYawMax = 0.3f; // 반동 좌우 최대값

};
